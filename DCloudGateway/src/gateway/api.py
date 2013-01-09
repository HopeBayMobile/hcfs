import sys
import csv
import json
import os
import ConfigParser
import common
import subprocess
import time
import re
import signal
import urllib2
from datetime import datetime
from gateway import snapshot
import api_restore_conf
from common import S3QL_CACHE_DIR

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

################################################################################
# Configuration

# Global section
# Timeout for running system commands
RUN_CMD_TIMEOUT = 15

# Switch of enabling showing some debug log
enable_log = False

# script of s3qlstat
CMD_CHK_STO_CACHE_STATE = "sudo python /usr/local/bin/s3qlstat /mnt/cloudgwfiles"

# Which NIC to monitoring
MONITOR_IFACE = "eth1"

# Samba section
# Samba's conf file
smb_conf_file = "/etc/samba/smb.conf"

#Samba's defualt user and passwd
default_user_id = "superuser"
default_user_pwd = "superuser"

# script of setting smb user's passwd
CMD_CH_SMB_PWD = "%s/change_smb_pwd.sh" % DIR

# NFS section
# NFS's ACL conf file
nfs_hosts_allow_file = "/etc/hosts.allow"

# Log section

# source of log files. There are too much info from the syslog,mount,fsck logs.
# We don't use them. Instead, we use gateway log, where records the results of
# invoking gateway's API.
LOGFILES = {
            #"syslog" : "/var/log/syslog",
            #"mount" : "/root/.s3ql/mount.log",
            #"fsck" : "/root/.s3ql/fsck.log",
            "gateway": "/var/log/delta/Gateway.log"
            }

# ****Note that not all logs will be displayed.****
# Only those with certain KEYWORDs that is defined by KEYWORD_FILTER, in log
# content will be shown and classified into three categories error,warning,info
KEYWORD_FILTER = {
                  "error_log": ["\[0\]"], # ["error", "exception"], # 0
                  "warning_log": ["\[1\]"], # ["warning"], # 1
                  "info_log": ["\[2\]"], # ["nfs", "cifs" , "."], #2
                  # the pattern . matches any log,
                  # that is if a log mismatches 0 or 1, then it will be assigned to 2
                  }

# This is used to eliminate the prefix of a log msg.
LOG_LEVEL_PREFIX_LEN = 4  # len("[0] ")

# Log files use different timestamp format. To unify the format, the LOG_PARSER defines
# regexp string for a log format. Log msg that mismatch the format will be ignored.
LOG_PARSER = {
             #"syslog" : re.compile("^(?P<year>[\d]?)(?P<month>[a-zA-Z]{3})\s+(?P<day>\d\d?)\s(?P<hour>\d\d)\:(?P<minute>\d\d):(?P<second>\d\d)(?:\s(?P<suppliedhost>[a-zA-Z0-9_-]+))?\s(?P<host>[a-zA-Z0-9_-]+)\s(?P<process>[a-zA-Z0-9\/_-]+)(\[(?P<pid>\d+)\])?:\s(?P<message>.+)$"),
             #"mount" : re.compile("^(?P<year>[\d]{4})\-(?P<month>[\d]{2})\-(?P<day>[\d]{2})\s+(?P<hour>[\d]{2})\:(?P<minute>[\d]{2}):(?P<second>[\d]{2})\.(?P<ms>[\d]+)\s+(\[(?P<pid>[\d]+)\])\s+(?P<message>.+)$"),
             #"fsck" : re.compile("^(?P<year>[\d]{4})\-(?P<month>[\d]{2})\-(?P<day>[\d]{2})\s+(?P<hour>[\d]{2})\:(?P<minute>[\d]{2}):(?P<second>[\d]{2})\.(?P<ms>[\d]+)\s+(\[(?P<pid>[\d]+)\])\s+(?P<message>.+)$"),
             "gateway": re.compile("^\[(?P<year>[\d]{4})\-(?P<month>[\d]{2})\-(?P<day>[\d]{2})\s+(?P<hour>[\d]{2})\:(?P<minute>[\d]{2}):(?P<second>[\d]{2}),(?P<ms>[\d]+)\]\s+(?P<message>.+)$"),
             }
# define how many categories of log are shown.
# 0 => show all logs, 2 => show only error log
SHOW_LOG_LEVEL = {
            0: ["error_log", "warning_log", "info_log"],
            1: ["error_log", "warning_log"],
            2: ["error_log"],
            }

# How many log records to show
NUM_LOG_LINES = 1024

#Snapshot tag
snapshot_tag = "/root/.s3ql/.snapshotting"
# var to remember s3ql fail/success counts
g_s3ql_fail_count = 0
g_s3ql_success_count = 0
# var to remember previous s3ql status
g_s3ql_on = True

################################################################################


class BuildGWError(Exception):
    pass


class EncKeyError(Exception):
    pass


class MountError(Exception):
    pass


class TestStorageError(Exception):
    pass


class GatewayConfError(Exception):
    pass


class UmountError(Exception):
    pass


class SnapshotError(Exception):
    pass
    
class NetworkError(Exception):
    pass

class UnauthorizedError(Exception):
    pass

def getGatewayConfig():
    """
    Get gateway configuration from /etc/delta/Gateway.ini.

    Will raise GatewayConfError if some error ocurred.

    @rtype: ConfigParser
    @return: Instance of ConfigParser.
    """

    try:
        config = ConfigParser.ConfigParser()
        with open('/etc/delta/Gateway.ini', 'rb') as fh:
            config.readfp(fh)

        if not config.has_section("mountpoint"):
            raise GatewayConfError("Failed to find section [mountpoint] in the config file")

        if not config.has_option("mountpoint", "dir"):
            raise GatewayConfError("Failed to find option 'dir'  in section [mountpoint] in the config file")

        if not config.has_section("network"):
            raise GatewayConfError("Failed to find section [network] in the config file")

        if not config.has_option("network", "iface"):
            raise GatewayConfError("Failed to find option 'iface' in section [network] in the config file")

        if not config.has_section("s3ql"):
            raise GatewayConfError("Failed to find section [s3q] in the config file")

        if not config.has_option("s3ql", "mountOpt"):
            raise GatewayConfError("Failed to find option 'mountOpt' in section [s3q] in the config file")

        if not config.has_option("s3ql", "compress"):
            raise GatewayConfError("Failed to find option 'compress' in section [s3q] in the config file")

        return config
    except IOError:
        op_msg = 'Failed to access /etc/delta/Gateway.ini'
        raise GatewayConfError(op_msg)

# wthung, 2013/1/3
def _show_led(status):
    """
    Show LED status.
    
    @type status: Integer
    @param status: System status passing to interval script.
    """
    led_script = '/etc/delta/LED_controller.sh'
    os.system('sudo %s %d' % (led_script, status))
        
# wthung, 2013/1/2
def _notify_savebox(status, msg):
    """
    Notify SAVEBOX via Restful API.
    
    @type status: Integer
    @param status: Status code of notification
    @type msg: String
    @param msg: Notification message
    @rtype: JSON object
    @return: Result of the notification
            - code: Return code of restful API
            - response: Response of restful API
    """
    # format current time
    now = datetime.utcnow()
    ts = '%d/%d/%d %d:%d:%d' % (now.year, now.month, now.day,
                             now.hour, now.minute, now.second)
    
    values = {"status": status,
              "timestamp": ts,
              "msg": msg}
    
    data = json.dumps(values)
    post_url = "http://127.0.0.1:8000/witch/services/system/reportSystemStatus/post"
    req = urllib2.Request(post_url, data, {'Content-Type': 'application/json'})
    code = -1
    response = None

    try:
        f = urllib2.urlopen(req)
        code = f.getcode()
        response = f.read()
        f.close()
    except urllib2.HTTPError as e:
        code = e.code
        response = e.read()
    except urllib2.URLError as e:
        response = e.reason
    except Exception as e:
        response = str(e)
    
    if code != 200:
        log.warning('Failed to report system status.')
        log.debug('Failed to report system status. HTTP code = %d, response = %s' % (code, response))
    
    return {"code": code, "response": response}

# wthung, 2012/12/10
def getSaveboxConfig():
    """
    Get SAVEBOX configuration from /etc/delta/savebox.ini.

    Will raise GatewayConfError if some error ocurred.

    @rtype: ConfigParser
    @return: Instance of ConfigParser.
    """
    config_name = '/etc/delta/savebox.ini'

    try:
        config = ConfigParser.ConfigParser()
        with open(config_name, 'rb') as fh:
            config.readfp(fh)

        if not config.has_section("squid3"):
            raise GatewayConfError("Failed to find section [squid3] in the config file")
        if not config.has_option("squid3", "start_on_boot"):
            raise GatewayConfError("Failed to find option 'start_on_boot'  in section [squid3] in the config file")

        return config
    except IOError:
        op_msg = 'Failed to access %s' % config_name
        raise GatewayConfError(op_msg)

# wthung, 2012/12/26
def _get_s3ql_db_name():
    """
    Get S3QL metadata database name.
    
    @rtype: string
    @return: Name of S3QL metadata database
    """
    db_name = None
    
    try:
        config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            config.readfp(op_fh)
    
        section = "CloudStorageGateway"
        account = config.get(section, 'backend-login')
        account, username = account.split(':')
        storage_url = config.get(section, 'storage-url').replace("swift://", "")
        
        # list all content in /root/.s3ql
        for item in os.listdir('/root/.s3ql'):
            if item.find('.db') != -1 and item.find('swift') != -1 \
                and item.find(username) != -1 and item.find(storage_url) != -1 \
                and item.find('wal') == -1:
                db_name = item
    except Exception as e:
        #log.error('Failed to get S3QL metadata DB name: %s.' % str(e))
        print('Failed to get S3QL metadata DB name: %s.' % str(e))
    finally:
        return db_name
        
# wthung
def _get_storage_account():
    """
    Get storage account from auth file.
    @rtype: string
    @return: Storage account or "" if failed.
    """
    
    op_account = ""
    
    try:
        op_config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            op_config.readfp(op_fh)
    
        section = "CloudStorageGateway"
        op_account = op_config.get(section, 'backend-login')
    except Exception as e:
        log.debug("Failed to _get_storage_account: %s" % str(e))
    
    return op_account

def getStorageUrl():
    """
    Get storage URL from /root/.s3ql/authinfo2.

    @rtype: string
    @return: Storage URL or None if failed.
    """

    log.debug("getStorageUrl start")
    storage_url = None

    try:
        config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            config.readfp(op_fh)

        section = "CloudStorageGateway"
        storage_url = config.get(section, 'storage-url').replace("swift://", "")
    except Exception as e:
        log.debug("Failed to getStorageUrl: %s" % str(e))
    finally:
        log.debug("getStorageUrl end")
    return storage_url


def get_compression():
    """
    Get status of compression switch.

    @rtype: JSON object
    @return: Compress switch.

        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object.
            - switch: True if compression is ON. Otherwise, false.
    """

    log.debug("get_compression start")
    op_ok = False
    op_msg = ''
    op_switch = True

    try:
        config = getGatewayConfig()
        compressOpt = config.get("s3ql", "compress")
        if compressOpt == "false":
            op_switch = False

        op_ok = True
        op_msg = "Succeeded to get_compression"

    except GatewayConfError as e:
        op_msg = str(e)
    except Exception as e:
        op_msg = str(e)
    finally:
        if not op_ok:
            log.error(op_msg)

        return_val = {'result': op_ok,
                      'msg': op_msg,
                      'data': {'switch': op_switch}}

        log.debug("get_compression end")
    return json.dumps(return_val)


def _check_snapshot_in_progress():
    """
    Check if the tag /root/.s3ql/.snapshotting exists.

    @rtype: boolean
    @return: True if snapshotting is in progress. Otherwise false.
    """

    try:
        if os.path.exists(snapshot_tag):
            return True
        return False
    except:
        raise SnapshotError("Could not decide whether a snapshot is in progress.")


def _check_s3ql():
    """
    Check if s3ql is correctly mounted, and if /mnt/cloudgwfiles exists in mount table

    @rtype: boolean
    @return: True if S3QL is healthy.
    """
    try:
        if _check_process_alive('mount.s3ql'):
            cmd = "sudo df"
            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, preexec_fn=os.setsid)
            countdown = 30
            while countdown > 0:
                po.poll()
                if po.returncode != 0:
                    countdown = countdown - 1
                    if countdown <= 0:
                        pogid=os.getpgid(po.pid)
                        os.system('kill -9 -%s' % pogid)
                        break
                    else:
                        time.sleep(1)
                else:
                    output = po.stdout.read()
                    if output.find("/mnt/cloudgwfiles") != -1:
                        return True
                    break
    except:
        pass
    return False

def _check_s3ql_writing(stat):
    """
    Check if S3QL is allowed to write.
    
    @rtype: boolean
    @return: True if S3QL can be written.
    """
    if stat.find('File system writing: On') != -1:
        return True
    return False
    
    
def get_indicators():
    """
    Get gateway services' indicators by calling internal functions.
    This function may return up to several seconds.
    Use it carefully in environment which needs fast response time.

    This function may be called by gateway background task.

    @rtype: dictionary
    @return: Gateway services' indicators.

        - result: Function call result.
        - msg: Explanation of result.
        - data: dictionary
            - network_ok: If network from gateway to storage is up.
            - system_check: If fsck.s3ql daemon is alive.
            - flush_inprogress: If S3QL dirty cache flushing is in progress.
            - dritycache_nearfull: If S3QL dirty cache is near full.
            - HDD_ok: If all HDD are healthy.
            - NFS_srv: If NFS is alive.
            - SMB_srv: If Samba service is alive.
            - snapshot_in_progress: If S3QL snapshotting is in progress.
            - HTTP_proxy_srv: If HTTP proxy server is alive.
            - S3QL_ok: If S3QL service is running.
            - S3QL_writing: If S3QL is allowed to write.
    """
    global g_s3ql_fail_count
    global g_s3ql_success_count
    global g_s3ql_on
    
    op_ok = False
    op_code = 0x8014
    op_msg = 'Reading SAVEBOX indicators failed.'
    return_val = {
          'result' : op_ok,
          'msg'    : op_msg,
          'code'   : op_code,
          'data'   : {'network_ok' : False,
          'system_check' : False,
          'flush_inprogress' : False,
          'dirtycache_nearfull' : False,
          'HDD_ok' : False,
          'NFS_srv' : False,
          'SMB_srv' : False,
          'snapshot_in_progress' : False,
          'HTTP_proxy_srv' : False,
          'S3QL_ok': False,
          'S3QL_writing': True}}

    try:
        op_s3ql_ok = False
        # get s3ql statistics by s3qlstat
        ret_code, output = _run_subprocess('sudo s3qlstat /mnt/cloudgwfiles', 15)
        # s3qlstat return 0 in success, 1 in failure
        if ret_code == 0:
            op_network_ok = _check_network()
            op_system_check = _check_process_alive('fsck.s3ql')
            op_flush_inprogress = _check_flush(output)
            op_dirtycache_nearfull = _check_dirtycache(output)
            op_HDD_ok = _check_HDD()
            op_NFS_srv = _check_nfs_service()
            op_SMB_srv = _check_smb_service()
            op_snapshot_in_progress = _check_snapshot_in_progress()
            op_Proxy_srv = _check_process_alive('squid3')
            op_s3ql_ok = _check_s3ql()
            op_s3ql_writing = _check_s3ql_writing(output)

            op_ok = True
            op_code = 0x8
            op_msg = "Reading SAVEBOX indicators was successful."
        
            return_val = {
                  'result' : op_ok,
                  'msg'    : op_msg,
                  'code'   : op_code,
                  'data'   : {'network_ok' : op_network_ok,
                  'system_check' : op_system_check,
                  'flush_inprogress' : op_flush_inprogress,
                  'dirtycache_nearfull' : op_dirtycache_nearfull,
                  'HDD_ok' : op_HDD_ok,
                  'NFS_srv' : op_NFS_srv,
                  'SMB_srv' : op_SMB_srv,
                  'snapshot_in_progress' : op_snapshot_in_progress,
                  'HTTP_proxy_srv' : op_Proxy_srv,
                  'S3QL_ok': op_s3ql_ok,
                  'S3QL_writing': op_s3ql_writing}}
        if not op_s3ql_ok:
            g_s3ql_fail_count += 1
            g_s3ql_success_count = 0
            if g_s3ql_fail_count >= 3:
                g_s3ql_fail_count = 0
                if g_s3ql_on:
                    # inform savebox to shutdown their services
                    log.debug('S3QL is down. Notify SAVEBOX to shut down services.')
                    _notify_savebox(3, "SAVEBOX is not ready.")
                    g_s3ql_on = False
        else:
            g_s3ql_success_count += 1
            g_s3ql_fail_count = 0
            if g_s3ql_success_count >= 3:
                g_s3ql_success_count = 0
                if not g_s3ql_on:
                    # inform savebox to restart their services
                    log.debug('S3QL is up again. Notify SAVEBOX to restart services.')
                    _notify_savebox(0, "SAVEBOX is ready.")
                    g_s3ql_on = True
    except Exception as Err:
        log.error("Unable to get indicators")
        log.error("msg: %s" % str(Err))
        return return_val

    return return_val

# by Rice
# modified by wthung, 2012/11/26
def get_gateway_indicators():
    """
    Get gateway services' indicators by reading file or by the result of calling get_indicators().
    
    @rtype: JSON object
    @return: Gateway services' indicators.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object.
            - network_ok: If network from gateway to storage is up.
            - system_check: If fsck.s3ql daemon is alive.
            - flush_inprogress: If S3QL dirty cache flushing is in progress.
            - dritycache_nearfull: If S3QL dirty cache is near full.
            - HDD_ok: If all HDD are healthy.
            - NFS_srv: If NFS is alive.
            - SMB_srv: If Samba service is alive.
            - snapshot_in_progress: If S3QL snapshotting is in progress.
            - HTTP_proxy_srv: If HTTP proxy server is alive.
            - S3QL_ok: If S3QL service is running.
            - S3QL_writing: If S3QL is allowed to write.
            - uplink_usage: Network traffic going from gateway.
            - downlink_usage: Network traffic coming to gateway.
    """

    op_ok = False
    op_code = 0x8014
    op_msg = 'Reading SAVEBOX indicators failed due to unexpected errors.'

    # Note: indicators and net speed are acuquired from different location
    #       don't mess them up
    return_val = {
          'result' : op_ok,
          'msg'    : op_msg,
          'code'   : op_code,
          'data'   : {'network_ok' : False,
          'system_check' : False,
          'flush_inprogress' : False,
          'dirtycache_nearfull' : False,
          'HDD_ok' : False,
          'NFS_srv' : False,
          'SMB_srv' : False,
          'snapshot_in_progress' : False,
          'HTTP_proxy_srv' : False,
          'S3QL_ok': False,
          'S3QL_writing': True}}
    return_val2 = {
          'uplink_usage' : 0,
          'downlink_usage' : 0}

    
    # indicator file
    indic_file = '/dev/shm/gw_indicator'
            
    try:
        # check if indicator file is exist
        if os.path.exists(indic_file):
            # read indicator file as result
            # deserialize json object from file
            with open(indic_file) as fh:
                return_val = json.load(fh)
                # update proxy status
                return_val['data']['HTTP_proxy_srv'] = _check_process_alive('squid3')
        else:
            # invoke regular function calls
            log.info('No indicator file existed. Try to spend some time to get it')
            return_val = get_indicators()

        # below call already checks netspeed indic file
        return_val2 = get_network_speed(MONITOR_IFACE)
           
    except Exception as Err:
        log.error("msg: %s" % str(Err))
        return_val['data'].update(return_val2)
        return json.dumps(return_val)

    return_val['data'].update(return_val2)
    return json.dumps(return_val)

# Code written by Jiahong Wu (traceroute)
def _traceroute_backend(backend_IP=None):
    """
    Return a traceroute message from gateway to backend

    @type backend_IP: string
    @param backend_IP: IP of backend. If the value is None, the url stored in authinfo2 is used.
    @rtype: string
    @return: traceroute info from gateway to backend
    """

    op_msg = "Unable to obtain traceroute info to the backend"
    log.debug("_traceroute_backend start")
    try:
        if backend_IP == None:
            op_config = ConfigParser.ConfigParser()
            with open('/root/.s3ql/authinfo2') as op_fh:
                op_config.readfp(op_fh)

            section = "CloudStorageGateway"
            op_storage_url = op_config.get(section, 'storage-url').replace("swift://", "")
            index = op_storage_url.find(":")
            if index != -1:
                op_storage_url = op_storage_url[0:index]
        else:
            op_storage_url = backend_IP

        cmd = "sudo traceroute %s" % op_storage_url
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode == 0:
            op_msg = "Traceroute message to the backend as follows:\n" + output
        else:
            op_msg = op_msg + '\nBackend url: ' + op_storage_url

    except IOError as e:
            op_msg = 'Unable to access /root/.s3ql/authinfo2.'
            log.error(str(e))
    except Exception as e:
            op_msg = 'Unable to obtain storage url or login info.'
            log.error(str(e))

    finally:
        log.debug("_traceroute_backend end")
    return op_msg


# check network connection from gateway to storage by Rice
def _check_network():
    """
    Check network connection from gateway to storage.
    
    @rtype: boolean
    @return: True if network is alive. Otherwise false.
    """
    
    op_network_ok = False
    try:
        op_config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            op_config.readfp(op_fh)

        section = "CloudStorageGateway"
        op_storage_url = op_config.get(section, 'storage-url').replace("swift://", "")
        full_storage_url = op_storage_url
        index = op_storage_url.find(":")
        if index != -1:
            op_storage_url = op_storage_url[0:index]

        cmd = "sudo ping -c 5 %s" % op_storage_url
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
    
        if po.returncode == 0:
            if output.find("icmp_req" and "ttl" and "time") != -1:
                # Jiahong: Add check to see if swift is working
                op_user = op_config.get(section, 'backend-login')
                op_pass = op_config.get(section, 'backend-password')

                # wthung, 2012/9/6
                # get user name
                _, username = op_user.split(':')

                cmd = "sudo swift -A https://%s/auth/v1.0 -U %s -K %s stat %s_private_container" % (full_storage_url, op_user, op_pass, username)
                ret_code, output = _run_subprocess(cmd, 30)
                if ret_code == 0:
                    if output.find("Bytes:") != -1:
                        op_network_ok = True
        else:
            log.info(output)

    except IOError as e:
            log.error('Unable to access /root/.s3ql/authinfo2.')
            log.error(str(e))
    except Exception as e:
            log.error('Unable to obtain storage url or login info.')
            log.error(str(e))
    finally:
        pass
    
    return op_network_ok

# wthung, 2012/7/17
# check input process name is running
def _check_process_alive(process_name=None):
    """
    Check if input process is running.
    
    @rtype: boolean
    @return: True if input process is alive. Otherwise false.
    """

    op = False

    if process_name is not None:
        try:
            cmd = "sudo ps aux | grep %s" % process_name
            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            lines = po.stdout.readlines()
            po.wait()

            if po.returncode == 0:
                if len(lines) > 2:
                    op = True
            else:
                log.info(lines)
        except:
            pass

    return op

# flush check by Rice
def _check_flush(stat):
    """
    Check if S3QL dirty cache flushing is in progress.
    
    @rtype: boolean
    @return: True if dirty cache flushing is in progress. Otherwise false.
    """
    if stat.find("Cache uploading: On") != -1:
        return True
    return False

# dirty cache check by Rice
def _check_dirtycache(stat):
    """
    Check if S3QL dirty cache is near full.
    
    @rtype: boolean
    @return: True if dirty cache is near full. Otherwise false.
    """
    if stat.find("Dirty cache near full: True") != -1:
        return True
    return False

def _get_serial_number(disk):
    """
    Get disk serial number by hdparm utility.
    
    @type disk: string
    @param disk: HDD device name. e.g., /dev/sda
    @rtype: string
    @return: The serial number of input HDD device. Return '00000000' if failed.
    """
    
    target_str = 'SerialNo='
    sn = '00000000'
    
    try:
        cmd = "sudo hdparm -i %s" % disk
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.readlines()
        po.wait()
    
        if po.returncode == 0:
            # grep the serial number
            for line in output:
                if target_str not in line:
                    continue
                
                index = line.find(target_str)
                sn = line[index + len(target_str):]
                break
        else:
            log.error('Some error occurred when getting serial number of %s' % disk)
    except:
        pass
    
    return sn.rstrip()

# check disks on gateway by Rice
def _check_HDD():
    """
    Check HDD healthy status by S.M.A.R.T..
    
    @rtype: boolean
    @return: True if all HDD are healthy. Otherwise false.
    """
    
    op_HDD_ok = False
    op_disk_num = True

    try:
        all_disk = common.getAllDisks()
        nu_all_disk = len(all_disk)
        op_all_disk = 0
        
        # wthung, 2012/7/18
        # check if hdds number is 3. If not, report the serial number of alive hdd to log
        # wthung, 2012/10/3, now we only have 2 hdds
        if nu_all_disk < 2:
            log.error('Some disks were lost. Please check immediately')
            for disk in all_disk:
                disk_sn = _get_serial_number(disk)
                log.error('Alive disk serial number: %s' % disk_sn)
            op_disk_num = False
    
        for i in all_disk:
            cmd = "sudo smartctl -a %s" % i
        
            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()

            if output.find("SMART overall-health self-assessment test result: PASSED") != -1:
                op_all_disk += 1 
            else:
                log.error("%s (SN: %s) SMART test result: NOT PASSED" % (i, _get_serial_number(i)))
        
        if (op_all_disk == len(all_disk)) and op_disk_num:
            op_HDD_ok = True

    except:
        pass

    return op_HDD_ok

# check nfs daemon by Rice
def _check_nfs_service():
    """
    Check NFS daemon.
    
    @rtype: boolean
    @return: True if NFS is alive. Otherwise false.
    """
    
    op_NFS_srv = True

    try:
        cmd = "sudo service nfs-kernel-server status"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
    
        # if nfsd is running, command returns 0
        # if nfsd is not running, command returns 3
        if po.returncode == 3:
            if output.find("not running") >= 0:
                op_NFS_srv = False
        else:
            pass

    except:
        pass

    log.debug("_check_nfs_service end")
    return op_NFS_srv

# check samba daemon by Rice
def _check_smb_service():
    """
    Check Samba and NetBIOS daemon.
    
    @rtype: boolean
    @return: True if Samba and NetBIOS service are all alive. Otherwise false.
    """

    op_SMB_srv = False

    try:
        cmd = "sudo service smbd status"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        # no matter smbd is running or not, command always returns 0
        if po.returncode == 0:
            if output.find("running") != -1:
                op_SMB_srv = True
        else:
            log.error(output)

        # if samba service is running, go check netbios
        if op_SMB_srv:
            cmd2 = "sudo service nmbd status"
            po2 = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output2 = po2.stdout.read()
            po2.wait()

            if po2.returncode == 0:
                if output2.find("running") == -1:
                    op_SMB_srv = False
            else:
                log.error(output)

    except:
        pass
    return op_SMB_srv

def get_HDD_status():
    """
    Get HDD_status.
    
    @rtype: JSON object
    @return: Result of getting HDD status .
    
        
        -result : whether this return is correct or not.
        -msg : It is supposed only be used by Delta.
        -msg_code : It is an optional field for mapping message code to i18n strings.
        -data : data contain an HDD object array
                [   
                {
                        "serial": "string",
                        "status": 0 // normal, 1 // rebuiling RAID, 2 // failed, 3 // not installed 
                },
                ...
                ]
    """
    return_val = {
        'result': False,
        'msg': '',
        'msg_code': '',
        'data': '',
        }
            
    try:
        with open("/root/gw_HDD_status","r") as fh:
            return_val = json.loads(fh.read())

    except:
        pass
        
    return json.dumps(return_val)

def get_storage_account():
    """
    Get storage account.
    
    @rtype: JSON object
    @return: Result of getting storage account and storage URL with account name if success.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object.
            - storage_url: storage URL which can be specified in 
              either domain name or IP, followed by port number.
            - account: The storage account name.
    """
    
    log.debug("get_storage_account start")

    op_ok = False
    op_code = 0x8016
    op_msg = 'Reading storage account failed.'
    op_storage_url = ''
    op_account = ''

    try:
        op_config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            op_config.readfp(op_fh)

        section = "CloudStorageGateway"
        op_storage_url = op_config.get(section, 'storage-url').replace("swift://", "")
        op_account = op_config.get(section, 'backend-login')
        op_ok = True
        op_code = 0xD
        op_msg = 'Obtaining storage account information was successful.'

    except IOError as e:
        op_code = 0x8003
        op_msg = 'File access failed.'
        log.debug(str(e))
    except Exception as e:
        op_code = 0x8017
        op_msg = 'Obtaining storage url or login info failed.' 
        log.error(str(e))

    return_val = {'result' : op_ok,
             'msg' : op_msg,
             'code': op_code,
             'data' : {'storage_url' : op_storage_url, 'account' : op_account}}

    log.debug("get_storage_account end")
    return json.dumps(return_val)

def apply_storage_account(storage_url, account, password, test=True):
    """
    Apply for a storage account.
    First call test_storage_account for storage credential, 
    then write the info to the configuration file if credential is good.
    
    @type storage_url: string
    @param storage_url: Storage URL.
    @type account: string
    @param account: Account name.
    @type password: string
    @param password: Account password.
    @type test: boolean
    @param test: Test account prior to apply.
    @rtype: JSON object
    @return: Result of applying storage account.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    log.debug("apply_storage_account start")

    op_ok = False
    op_code = 0x8002
    op_msg = 'Applying storage account failed.'

    if test:
        test_gw_results = json.loads(test_storage_account(storage_url, account, password))
        if not test_gw_results['result']:
            return json.dumps(test_gw_results)

    try:
        op_config = ConfigParser.ConfigParser()
        #Create authinfo2 if it doesn't exist
        if not os.path.exists('/root/.s3ql/authinfo2'):
            os.system("sudo mkdir -p /root/.s3ql")
            os.system("sudo touch /root/.s3ql/authinfo2")
            os.system("sudo chown www-data:www-data /root/.s3ql/authinfo2")
            os.system("sudo chmod 600 /root/.s3ql/authinfo2")
    
        with open('/root/.s3ql/authinfo2', 'rb') as op_fh:
            op_config.readfp(op_fh)
    
        section = "CloudStorageGateway"
        if not op_config.has_section(section):
            op_config.add_section(section)
    
        op_config.set(section, 'storage-url', "swift://%s" % storage_url)
        op_config.set(section, 'backend-login', account)
        op_config.set(section, 'backend-password', password)
    
        with open('/root/.s3ql/authinfo2', 'wb') as op_fh:
            op_config.write(op_fh)
            
        # get user default container name
        user_container = _get_user_container_name(account)

        # wthung, 2012/8/16
        # re-create upstart s3ql.conf
        _createS3qlConf(storage_url, user_container)
        
        op_ok = True
        op_code = 0x2
        op_msg = 'Applying storage account was successful.'

    except IOError as e:
        op_code = 0x8003
        op_msg = 'File access failed.'
        log.error(str(e))
    except Exception as e:
        log.error(str(e))

    return_val = {'result' : op_ok,
          'msg': op_msg,
          'code': op_code,
          'data': {}}

    log.debug("apply_storage_account end")
    return json.dumps(return_val)

def apply_user_enc_key(old_key=None, new_key=None):
    """
    Apply a new encryption passphrase.
    
    @type old_key: string
    @param old_key: Old encryption passphrase.
    @type new_key: string
    @param new_key: New encryption passphrase.
    @rtype: JSON object
    @return: Result of applying new passphrase.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    log.debug("apply_user_enc_key start")

    op_ok = False
    op_code = 0x800A
    op_msg = 'Changing encryption key failed.'
    
    def do_change_passphrase():
        storage_url = op_config.get(section, 'storage-url')
        cmd = "sudo python /usr/local/bin/s3qladm --cachedir /root/.s3ql passphrase %s/%s/delta" % (storage_url, user_container)
        po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (stdout, stderr) = po.communicate(new_key)
        if po.returncode != 0:
            if stdout.find("Wrong bucket passphrase") != -1:
                op_code = 0x8009
                op_msg = "The old key stored in the authentication file is not correct."
            else:
                op_code = 0x800A
                op_msg = "Changing encryption key failed."
                log.error(stdout)
            raise Exception(op_msg)
        
        op_config.set(section, 'bucket-passphrase', new_key)
        with open('/root/.s3ql/authinfo2', 'wb') as op_fh:
            op_config.write(op_fh)

    try:
        #Check if the new key is of valid format
        if not common.isValidEncKey(new_key):
            op_code = 0x8004
            op_msg = "The encryption key must be an alphanumeric string of length between 6 and 20."
            raise Exception(op_msg)
    
        op_config = ConfigParser.ConfigParser()
        if not os.path.exists('/root/.s3ql/authinfo2'):
            op_code = 0x8005
            op_msg = "File finding failed."
            raise Exception(op_msg)

        with open('/root/.s3ql/authinfo2', 'rb') as op_fh:
            op_config.readfp(op_fh)

        section = "CloudStorageGateway"
        if not op_config.has_section(section):
            op_code = 0x8006
            op_msg = "Section could not be found."
            raise Exception(op_msg)
    
        #TODO: deal with the case where the key stored in /root/.s3ql/authoinfo2 is Wrong
        key = op_config.get(section, 'bucket-passphrase')
        if key != old_key:
            op_code = 0x8007
            op_msg = "The old key is not correct."
            raise Exception(op_msg)
        # get user account
        account = op_config.get(section, 'backend-login')
        user_container = _get_user_container_name(account)
        if not user_container:
            op_code = 0x8008
            op_msg = "Errors occurred when getting user container name."
            raise Exception(op_msg)
    
        _umount()
        do_change_passphrase()
        
        op_ok = True
        op_code = 0x3
        op_msg = 'Applying new user encryption key was successful.'

    except IOError as e:
        op_code = 0x800C
        op_msg = 'Authentication file access failed.'
        log.error(str(e))
    except UmountError as e:
        op_code = 0x800D
        op_msg = "Unmounting failed."
        log.error(str(e))
        # undo the umount process
        _undo_umount()
    except common.TimeoutError as e:
        op_code = 0x800E
        op_msg = "Unmounting failed due to time out."
        # force to kill umount process
        _run_subprocess('pkill -9 umount.s3ql')
        # change passphrase
        do_change_passphrase()
    except Exception as e:
        log.error(str(e))
    finally:
        log.debug("apply_user_enc_key end")

        return_val = {'result': op_ok,
              'msg': op_msg,
              'code': op_code,
              'data': {}}

    return json.dumps(return_val)

def _createS3qlConf(storage_url, container):
    """
    Create S3QL configuration file and upstart script of gateway 
    by calling createS3qlconf.sh and createpregwconf.sh
    
    @type storage_url: string
    @param storage_url: Storage URL.
    @rtype: integer
    @return: 0 for success. Otherwise, nonzero value.
    """
    
    log.debug("_createS3qlConf start")
    ret = 1
    try:
        config = getGatewayConfig()
        mountpoint = config.get("mountpoint", "dir")
        mountOpt = config.get("s3ql", "mountOpt")
        iface = config.get("network", "iface")
        compress = "lzma" if config.get("s3ql", "compress") == "true" else "none"
        mountOpt = mountOpt + " --compress %s" % compress 
    
        cmd = 'sudo sh %s/createS3qlconf.sh %s %s %s "%s"' % (DIR, iface, "swift://%s/%s/delta" % (storage_url, container), mountpoint, mountOpt)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
    
        ret = po.returncode
        if ret != 0:
            log.error("Failed to create s3ql config for %s" % output)
                
        storage_component = storage_url.split(":")
        storage_addr = storage_component[0]

        cmd = 'sudo sh %s/createpregwconf.sh %s' % (DIR, storage_addr)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        ret = po.returncode
        if ret != 0:
            log.error("Failed to create s3ql config for %s" % output)

    except Exception as e:
        log.error("Failed to create s3ql config for %s" % str(e))
    finally:
        log.debug("_createS3qlConf end")
    return ret
        

def _get_user_container_name(account):
    """
    Get user default container name from input account:username pair
    
    @type account: string
    @param account: The account:username pair
    @rtype: string
    @return: User's default container name
    """
    
    # check string format
    if not ":" in account:
        log.error("Format error: Account name didn't contain ':'")
        return ""
    
    # try to get user name from account
    _, username = account.split(":")
    user_container = "%s_private_container" % username
    log.debug('container name %s for user %s' % (user_container, username))
    
    return user_container

@common.timeout(180)
def _check_container(storage_url, account, password):
    """
    Check user's default container.
    
    Will raise BuildGWError if failed.
    
    @type storage_url: string
    @param storage_url: Storage URL.
    @type account: string
    @param account: Account name.
    @type password: string
    @param password: Account password.
    """
    user_container = ''

    try:
        # get user default container name
        user_container = _get_user_container_name(account)
        
        cmd = "sudo swift -A https://%s/auth/v1.0 -U %s -K %s stat %s" % (storage_url, account, password, user_container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
    
        if po.returncode != 0:
            op_msg = "Failed to stat container: %s" % output
            raise Exception(op_msg)
        
        output = output.strip()
        if not "Bytes" in output:
            op_msg = "Failed to find user's container %s. Output=%s" % (user_container, output)
            log.error(op_msg)
            raise Exception(op_msg)
        else:
            log.debug('Found user container %s' % user_container)
    except Exception as e:
        log.error(str(e))
        raise BuildGWError(0x8027, "Checking user container failed.")
        
    return user_container

def _mkfs(storage_url, key, container):
    """
    Create S3QL file system.
    Do file system checking if an existing one was found.
    
    Will raise BuildGWError if failed.
    
    @type storage_url: string
    @param storage_url: Storage URL.
    @type key: string
    @param key: Encryption passphrase.
    @rtype: boolean
    @return: True if a file system is existed. Otherwise, False.
    """

    log.debug("_mkfs start")

    # wthung, 2012/8/3
    # add a var to indicate an exiting filesys
    has_existing_filesys = False

    try:
        cmd = "sudo python /usr/local/bin/mkfs.s3ql --cachedir /root/.s3ql --authfile /root/.s3ql/authinfo2 --max-obj-size 2048 swift://%s/%s/delta" % (storage_url, container)
        po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdout, stderr) = po.communicate(key)
        if po.returncode != 0:
            if stderr.find("existing file system!") == -1:
                op_msg = "Failed to mkfs for %s" % stderr
                log.error(op_msg)
                raise BuildGWError(0x8028, "Making SAVEBOX file system failed.")
            else:
                log.info("Found existing file system!")
                log.info("Conducting forced file system check")
                has_existing_filesys = True

                cmd = "sudo python /usr/local/bin/fsck.s3ql --batch --force --authfile /root/.s3ql/authinfo2 --cachedir /root/.s3ql swift://%s/%s/delta" % (storage_url, container)
                po = subprocess.Popen(cmd, shell=True, stderr=subprocess.STDOUT, preexec_fn=os.setsid)
                countdown = 86400
                while countdown > 0:
                    po.poll()
                    if po.returncode is None:
                        # fsck not finished
                        countdown = countdown - 1
                        if countdown <= 0:
                            log.error("Timed out during fsck.")
                            raise RuntimeError
                        else:
                            time.sleep(1)
                    else:
                        # fsck terminated, check return code again
                        if po.returncode != 0:
                            if po.returncode == 128:
                                # wrong passphrase
                                raise EncKeyError
                            raise RuntimeError
                        break
    except Exception as e:
        log.error('_mkfs error: %s' % str(e))
        raise

    log.debug("_mkfs end")
    return has_existing_filesys

# wthung, 2012/11/26
def _findChildPids(pid, pslist, cpid_list):
    """
    Find all children/grandchildren of input pid.
    This function is stictly based on result of command 'ps eo pid,pgid,ppid'.
    
    @type pid: integer
    @param pid: Target process ID
    @type pslist: list
    @param pslist: Process list
    @type cpid_list: list
    @param cpid_list: List of all children/grandchildren processes
    """
    for ps in pslist:
        if ps[2] == pid:
            cpid_list.append(ps[0])
            _findChildPids(ps[0], pslist, cpid_list)
    
    return cpid_list
    
# wthung, 2012/11/26
def _killChildrenProcess(pid):
    """
    Kill process of pid and all its children/grandchildren
    
    @type pid: integer
    @param pid: Target process ID
    """
    # get the pid, pgid, ppid of our current processes:
    command = "ps axo pid,pgid,ppid"
    psraw = os.popen(command).readlines()
    psList = []
    killList = []
    
    for ps in psraw[1:]: # 1: gets rid of header
        psList.append(map(int,ps.split()))
    
    killList.append(pid)
    _findChildPids(pid, psList, killList)
    
    for id in killList:
        os.system('sudo kill -9 %d' % id)
    
    return
    
# wthung, 2012/11/26
def _run_subprocess(cmd, timeout=86400):
    """
    Utility function to run a command by subprocess.Popen. 
    Kill the subprocess after input timeout.
    
    @type cmd: string
    @param cmd: Command to run
    @rtype: tuple
    @return: Command return code and output string in a tuple
    """
    output = ""
    po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    
    while True:
        # check if process stops
        po.poll()
        
        if po.returncode is None:
            # not yet stop
            timeout = timeout - 1
            if timeout <= 0:
                _killChildrenProcess(po.pid)
                break
            else:
                time.sleep(1)
        else:
            output = po.stdout.read()
            break
    
    return (po.returncode, output)

# wthung, 2012/10/16
@common.timeout(600)
def _undo_umount():
    """
    Undo the umount process
    
    Note:
        - Mount COSA related folders
        - Start NFS
        - Start NetBIOS
        - Start Samba
    """
    log.debug('Undo SAVEBOX umount process')
    
    # mount cosa related folders if necessary
    # note: we can't use os.path.ismount to check a binded folder
    ret_code, output = _run_subprocess("sudo mount | grep /COSASTORAGE/ALFRESCO")
    if ret_code:
        bind_path = '/mnt/cloudgwfiles/COSA /COSASTORAGE/ALFRESCO'
        ret_code, output = _run_subprocess("sudo mount -o bind %s" % bind_path)
        if ret_code:
            log.error('Unable to bind %s: %s' % (bind_path, output))
    
    # start nfs service
    ret_code, output = _run_subprocess("sudo /etc/init.d/nfs-kernel-server restart")
    if ret_code:
        log.error('Unable to start NFS service: %s' % output)
    
    # start nmbd
    ret_code, output = _run_subprocess("sudo /etc/init.d/nmbd restart")
    if ret_code:
        log.error('Unable to start nmbd service: %s' % output)
    
    # start smbd
    ret_code, output = _run_subprocess("sudo /etc/init.d/smbd restart")
    if ret_code:
        log.error('Unable to start smbd service: %s' % output)
    

@common.timeout(600)
def _umount():
    """
    Umount S3QL file system.
    
    Note:
        - Stop Samba.
        - Stop NetBIOS.
        - Stop NFS.
        - Umount.
    
    @rtype: boolean
    @return: True if succeed to umount file system. Otherwise, false.
    """
    log.debug("SAVEBOX umounting")
    op_ok = False

    try:
        config = getGatewayConfig()
        mountpoint = config.get("mountpoint", "dir")
    
        if os.path.ismount(mountpoint):
            # umount cosa related folders
            ret_code, output = _run_subprocess("sudo mount | grep /COSASTORAGE/ALFRESCO")
            if not ret_code:
                ret_code, output = _run_subprocess("sudo umount -l /COSASTORAGE/ALFRESCO")
                if ret_code:
                    raise UmountError(output)
            
            # stop smbd
            ret_code, output = _run_subprocess("sudo /etc/init.d/smbd stop")
            if ret_code:
                raise UmountError(output)

            # stop nmbd
            ret_code, output = _run_subprocess("sudo /etc/init.d/nmbd stop")
            if ret_code:
                raise UmountError(output)
            
            # stop nfs
            ret_code, output = _run_subprocess("sudo /etc/init.d/nfs-kernel-server stop")
            if ret_code:
                raise UmountError(output)

            # check if s3ql is mounted
            if _is_s3ql_mounted(mountpoint):
                # umount s3ql mount point
                ret_code, output = _run_subprocess("sudo python /usr/local/bin/umount.s3ql %s" % (mountpoint))
                if ret_code:
                    raise UmountError(output)
            
            op_ok = True
    except Exception as e:
        raise UmountError(str(e))
    
    finally:
        if op_ok == False:
            log.warning("Umounting SAVEBOX file system failed.")
        else:
            log.debug("Umounting SAVEBOX was successful.")

# wthung, 2012/1/8
def _mount_as_loop_ro(mountpoint):
    """
    Mount S3QL mount point to a loopback, read-only device, 
    so that a unexpected file operation won't succeed if S3QL is down.
    An entry will also be added into /etc/fstab.
    
    The input 'mountpoint' must be created and not mounted prior to this function.
    
    @type mountpoint: String
    @param mountpoint: Path of S3QL mount point
    """
    if os.path.exists(mountpoint) and (not os.path.ismount(mountpoint)):
        img_file = '/storage/ro.img'
        # create a dummy file using dd
        _run_subprocess('sudo dd if=/dev/zero of=%s bs=1024 count=500' % img_file, 20)
        # mkfs
        _run_subprocess('sudo mkfs -t ext4 -F %s' % img_file, 20)
        # mount
        _run_subprocess('sudo mount -t ext4 -o loop,ro %s %s' % (img_file, mountpoint), 20)
        # check if an entry for this mounting already existed
        ret_code, _ = _run_subprocess('sudo grep %s /etc/fstab' % img_file, 10)
        if ret_code == 1:
            # append an entry to /etc/fstab
            append_str = '# mount s3ql mount point as read-only fs\n/storage/ro.img /mnt/cloudgwfiles ext4 loop,ro 0 0'
            _run_subprocess('sudo echo "%s" >> /etc/fstab' % append_str, 10)

@common.timeout(360)
def _mount(storage_url, container):
    """
    Mount the S3QL file system.
    Will create S3QL configuration file and gateway upstart script before mounting.
        
    Will raise BuildGWError if failed.
    
    @type storage_url: string
    @param stroage_url: Storage URL.
    """
    
    log.debug("Mounting SAVEBOX")
    op_ok = False
    try:
        config = getGatewayConfig()
    
        mountpoint = config.get("mountpoint", "dir")
        mountOpt = config.get("s3ql", "mountOpt")
        compressOpt = "lzma" if config.get("s3ql", "compress") == "true" else "none"    
        mountOpt = mountOpt + " --compress %s" % compressOpt

        authfile = "/root/.s3ql/authinfo2"

        os.system("sudo mkdir -p %s" % mountpoint)
        # check if mount point is mounted
        if os.path.ismount(mountpoint):
            os.system("sudo umount %s" % mountpoint)
        # mount target mount point to a read-only loop fs
        _mount_as_loop_ro(mountpoint)

        #mount s3ql
        cmd = "sudo python /usr/local/bin/mount.s3ql %s --authfile %s --cachedir /root/.s3ql swift://%s/%s/delta %s" % (mountOpt, authfile, storage_url, container, mountpoint)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            if output.find("Wrong bucket passphrase") != -1:
                raise EncKeyError
            raise Exception(output)

        #mkdir in the mountpoint for smb share
        cmd = "sudo mkdir -p %s/sambashare" % mountpoint
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            raise Exception(output)

        #mkdir in the mountpoint for nfs share
        cmd = "sudo mkdir -p %s/nfsshare" % mountpoint
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            raise Exception(output)

        #change the owner of nfs share to nobody:nogroup
        cmd = "sudo chown nobody:nogroup %s/nfsshare" % mountpoint
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            raise Exception(output)

        op_ok = True
    except EncKeyError:
        raise
    except Exception as e:
        op_msg = "Failed to mount filesystem for %s" % str(e)
        log.error(str(e))
        raise BuildGWError(0x8029, "Mounting SAVEBOX file system failed.")

        if op_ok == False:
            log.error("Mounting SAVEBOX file system failed.")
        else:
            log.debug("Mounting SAVEBOX file system was successful.")
    

@common.timeout(360)
def _restartServices():
    """
    Restart all gateway services.
        - Samba
        - NetBIOS
        - NFS
    
    Will raise BuildGWError if failed.
    """
    
    log.debug("Restarting SAVEBOX services.")

    try:
        config = getGatewayConfig()
    
        cmd = "sudo /etc/init.d/smbd restart"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            op_msg = "Failed to start samba service for %s." % output
            raise Exception(op_msg)

        cmd = "sudo /etc/init.d/nmbd restart"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            op_msg = "Failed to start netbios service for %s." % output
            raise Exception(op_msg)

        cmd = "sudo /etc/init.d/nfs-kernel-server restart"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            op_msg = "Failed to start nfs service for %s." % output
            raise Exception(op_msg)

    except GatewayConfError as e:
        raise e, None, sys.exc_info()[2]

    except Exception as e:
        op_msg = "Failed to restart smb&nfs services for %s" % str(e)
        log.error(str(e))
        # look like this error won't be catched anymore, assign an valid err code to it
        raise BuildGWError(0x8999, "Restarting system services failed.")

    log.debug("Restarting SAVEBOX services was successful.")

# wthung, 2013/1/8
def _is_s3ql_mounted(mountpoint):
    """
    Check if S3QL is mounted on input 'mountpoint'.
    
    @type mountpoint: String
    @param mountpoint: Path to check
    """
    ret_code, _ = _run_subprocess('sudo s3qlstat %s' % mountpoint, 30)
    if ret_code == 0:
        return True
    return False

def build_gateway(user_key):
    """
    Build gateway. GUI need to call this function for the gateway initiation to start.
    
    Note:
        - Need to first test if there is an existing filesystem. If so, 
          use the provided auth info to check if can access the filesystem.
        - If no existing filesystem, need to call mkfs.s3ql to build S3QL filesystem, 
          write necessary files (see previous installation guide for stage 1 gateway doc, 
          minus the iSCSI part), and start/restart services.
        - Potential obstacle: In S3ql, the user encryption key is entered via keyboard, 
          and here we need to read it from a file.
        - Required configuration files: /root/.s3ql/authinfo2 (see s3ql doc for format).
    
    @type user_key: string
    @param user_key: Encryption passphrase for the file system.
    @rtype: JSON object
    @return: Result of building gateway.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    log.debug("Building SAVEBOX.")

    op_ok = False
    op_code = 0x8002
    op_msg = 'Applying storage account failed.'

    try:
        # wthung, 2012/8/15
        # first to check if mount point is available to avoid build gateway twice
        # original checking place is in _mount(). has removed it
        # wthung, 2013/1/8
        # change to use s3qlstat for checking
        config = getGatewayConfig()
        mountpoint = config.get("mountpoint", "dir")
        if _is_s3ql_mounted(mountpoint):
            raise BuildGWError(0x800F, "A file system was already mounted.")
            
        # wthung, 2012/12/25
        # in current wizard procedure, upstart script for s3ql and gateway were created by calling apply_storage_account
        # since build_gateway should be atomic operation, we move these two scripts to /root
        # in the last of this method, we'll create them again
        upstart_s3ql = '/etc/init/s3ql.conf'
        upstart_gw = '/etc/init/pre-gwstart.conf'
        current_time = int(time.time())
        if os.path.exists(upstart_s3ql):
            os.system('sudo mv %s /root/s3ql.conf.%d' % (upstart_s3ql, current_time))
        if os.path.exists(upstart_gw):
            os.system('sudo mv %s /root/pre-gwstart.conf.%d' % (upstart_gw, current_time))

        if not common.isValidEncKey(user_key):
            op_msg = "The encryption key must be an alphanumeric string of length between 6 and 20."
            raise BuildGWError(0x8004, op_msg)

        op_config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2', 'rb') as op_fh:
            op_config.readfp(op_fh)

        section = "CloudStorageGateway"
        op_config.set(section, 'bucket-passphrase', user_key)
        with open('/root/.s3ql/authinfo2', 'wb') as op_fh:
            op_config.write(op_fh)

        url = op_config.get(section, 'storage-url').replace("swift://", "")
        account = op_config.get(section, 'backend-login')
        password = op_config.get(section, 'backend-password')
        
        user_container = _check_container(storage_url=url, account=account, password=password)
        has_filesys = _mkfs(storage_url=url, key=user_key, container=user_container)
        _mount(storage_url=url, container=user_container)
        
        # wthung, 2012/8/3
        # if a file system is existed, try to rebuild snapshot database
        if has_filesys:
            log.info('Found existing file system. Try to restore configuration')
            # yen, 2012/10/09.
            # restore configuration from cloud
            api_restore_conf.restore_gateway_configuration()
            
            # wthung, 2012/12/10
            # read savebox.ini and set proxy status
            try:
                sb_config = getSaveboxConfig()
                proxy_status = sb_config.get('squid3', 'start_on_boot')
                if proxy_status == 'on':
                    # turn proxy on
                    os.system('service squid3 start')
                else:
                    os.system('service squid3 stop')
            except Exception as e:
                log.warning(str(e))
        
        # restart nfs/smb/nmb
        restart_service("nfs-kernel-server")
        restart_service("smbd")
        restart_service("nmbd")
        
        log.debug("setting upload speed")
        os.system("sudo /etc/cron.hourly/hourly_run_this")

        # update bandwidth to let s3ql uploadon
        os.system('/etc/delta/update_bandwidth')

        # we need to manually exec background task program,
        #   because it is originally launched by upstart 
        # launch background task program
        os.system("/usr/bin/python /etc/delta/gw_bktask.py")
        
        # create upstart script for s3ql and gateway
        # before this step, if gateway is reset accidently, build_gateway can be restarted
        if _createS3qlConf(url, user_container) != 0:
            raise BuildGWError(0x8026, "Creating SAVEBOX configuration failed.")
     
        op_ok = True
        op_code = 0x4
        op_msg = 'Building SAVEBOX was successful.'

    except common.TimeoutError:
        op_code = 0x8010
        op_msg = "Building SAVEBOX failed due to time out."
    except IOError as e:
        op_code = 0x8003
        op_msg = 'File access failed.'
    except EncKeyError as e:
        op_code = 0x8024
        op_msg = "The input encryption key is not correct."
    except BuildGWError as e:
        op_code, op_msg = e.args
    except Exception as e:
        op_msg = str(e)
    finally:
        if not op_ok:
            _umount()
            log.error(op_msg)
            log.debug("Building SAVEBOX failed. " + op_msg)
        else:
            log.debug("Building SAVEBOX was successful.")
            
        return_val = {'result' : op_ok,
                      'msg'    : op_msg,
                      'code'   : op_code,
                      'data'   : {}}

        return json.dumps(return_val)

def restart_nfs_service():
    """
    Restart NFS service.
    
    @rtype: JSON object
    @return: Result after restarting NFS service.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    log.debug("NFS service restarting")

    return_val = {}
    op_ok = False
    op_msg = "Restarting nfs service failed."

    try:
        cmd = "sudo /etc/init.d/nfs-kernel-server restart"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()
    
        if po.returncode == 0:
            op_ok = True
            op_msg = "Restarting nfs service succeeded."

    except Exception as e:
        op_ok = False
        log.error(str(e))
        log.error("NFS service restarting error")

    finally:
        return_val = {
            'result': op_ok,
            'msg': op_msg,
            'data': {}
        }
    
    log.debug("NFS service restarted")
    return json.dumps(return_val)

def restart_smb_service():
    """
    Restart Samba and NetBIOS services.
    
    @rtype: JSON object
    @return: Result after restarting Samba and NetBIOS services.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    log.debug("Samba service restarting")

    return_val = {}
    op_ok = False
    op_msg = "Restarting samba service failed."

    try:
        cmd = "sudo /etc/init.d/smbd restart"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()

        #Jiahong: Adding restarting nmbd as well
        cmd1 = "sudo /etc/init.d/nmbd restart"
        po1 = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po1.wait()

        if (po.returncode == 0) and (po1.returncode == 0):
            op_ok = True
            op_msg = "Restarting samba service succeeded."
            log.debug("Samba service restarted")

    except Exception as e:
        op_ok = False
        log.error(str(e))
        log.error("Samba service restarting error")

    finally:
        return_val = {
            'result': op_ok,
            'msg': op_msg,
            'data': {}
        }
    
        log.debug("restart_smb_service end")
    return json.dumps(return_val)

# wthung
def restart_service(svc_name):
    """
    Restart input service.
    
    @type svc_name: string
    @param svc_name: Service name to be restarted.
    @rtype: JSON object
    @return: Result after restarting the service.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    log.debug("%s service restarting" % svc_name)

    return_val = {}
    op_ok = False
    op_msg = "Restarting %s service failed." % svc_name

    try:
        # why we don't use 'service' utility?
        # because 'service nmbd restart' is invalid command.
        # thus, for simplicity, use /etc/init.d all the way
        cmd = "sudo /etc/init.d/%s restart" % svc_name
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()

        if po.returncode == 0:
            op_ok = True
            log.debug("%s service restarted successfully." % svc_name)
            op_msg = "Restarting %s service succeeded." % svc_name

    except Exception as e:
        op_ok = False
        log.error(str(e))
        log.error("%s service restarting error" % svc_name)

    finally:
        return_val = {
            'result': op_ok,
            'msg': op_msg,
            'data': {}
        }
    
        log.debug("%s service restarted" % svc_name)
    return json.dumps(return_val)

def reset_gateway():
    """
    Reset gateway by calling 'reboot' command directly.
    
    @rtype: JSON object
    @return: Result of resetting gateway.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    log.debug("Rebooting SAVEBOX.")

    return_val = {'result': True,
                  'msg': "Restarting SAVEBOX was successful.",
                  'code': 0xE,
                  'data': {}}
    
    try:
        pid = os.fork()
    
        if pid == 0:
            time.sleep(10)
            os.system("sudo reboot")
        else:
            log.debug("SAVEBOX will restart after ten seconds")
    except:
        pass
    
    return json.dumps(return_val)


def shutdown_gateway():
    """
    Shutdown gateway by calling 'poweroff' command directly.
    
    @rtype: JSON object
    @return: Result of shutting gateway down.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    log.debug("Shutdowning SAVEBOX.")
    
    return_val = {'result': True,
                  'msg': "Shutting down SAVEBOX was successful.",
                  'code': 0x11,
                  'data': {}}

    try:
        pid = os.fork()
    
        if pid == 0:
            time.sleep(10)
            os.system("sudo poweroff")
        else:
            log.debug("SAVEBOX will be shuted down after ten seconds")
    except:
        pass
    
    return json.dumps(return_val)


@common.timeout(30)
def _test_storage_account(storage_url, account, password):
    """
    Test the storage account to see if it works.
    Use auth process of swift to check for the credential of the account info.
    
    Will raise TestStorageError if failed.
    
    @type storage_url: string
    @param storage_url: Storage URL.
    @type account: string
    @param account: Account name.
    @type password: string
    @param password: Account password.
    """
    
    cmd = "sudo curl -k -v -H 'X-Storage-User: %s' -H 'X-Storage-Pass: %s' https://%s/auth/v1.0" % (account, password, storage_url)
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()
    ret_code = po.returncode
    
    comm_err = "Testing storage account failed."
    if ret_code != 0:
        # wthung, 2013/1/7
        # modifed to return more specified error message
        log.error(comm_err)
        log.error(output)
        if ret_code == 7:
            # curl's return code 7 == "Failed to connect to host."
            raise NetworkError
        raise TestStorageError(comm_err)
    
    if common.isHttpErr(output, 404):
        raise NetworkError
    
    if common.isHttpErr(output, 401):
        raise UnauthorizedError
    
    if not common.isHttp200(output):
        raise TestStorageError(comm_err)

def test_storage_account(storage_url, account, password):
    """
    Test the storage account to see if it works.
    Use auth process of swift to check for the credential of the account info.
    
    @type storage_url: string
    @param storage_url: Storage URL.
    @type account: string
    @param account: Account name.
    @type password: string
    @param password: Account password.
    @rtype: JSON object
    @return: Test result.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    log.debug("test_storage_account start")

    op_ok = False
    op_code = 0x8023
    op_msg = 'Testing storage account failed.'

    try:
        _test_storage_account(storage_url=storage_url, account=account, password=password)
        op_ok = True
        op_code = 0x14
        op_msg = 'Testing storage account was successful.'
        
    except common.TimeoutError:
        op_code = 0x8021
        op_msg = "Testing storage account failed due to time out." 
        log.error(op_msg)
    except TestStorageError as e:
        op_code = 0x8023
        op_msg = str(e)
        log.error(op_msg)
    except NetworkError as e:
        op_code = 0x8020
        op_msg = "Testing storage account failed due to network error."
        log.error(op_msg)
    except UnauthorizedError as e:
        op_code = 0x8025
        op_msg = "Testing storage account failed due to unauthorized error."
    except Exception as e:
        log.error(str(e))

    #Jiahong: Insert traceroute info in the case of a failed test
    if op_ok is False:
        try:
            url, _ = storage_url.split(':')
            traceroute_info = _traceroute_backend(url)
            log.error(traceroute_info)
            op_msg = op_msg + '\n' + traceroute_info
        except Exception as e:
            log.error('Error in traceroute:\n' + str(e))

    return_val = {'result' : op_ok,
                  'code'   : op_code,
                  'msg'    : op_msg,
                  'data'   : {}}
    
    log.debug("test_storage_account end")
    return json.dumps(return_val)

def get_network():
    """
    Get current storage gateway box network settings.
    
    @rtype: JSON object
    @return: The network setting of storage gateway.
        
        - result: Function call result
        - msg: Explanation of result
        - data: JSON object
            - ip: IP address
            - gateway: Network gateway
            - mask: Network mask
            - dns1: Primary DNS
            - dns2: Secondary DNS
    """
    
    log.debug("get_network start")

    info_path = "/etc/delta/network.info"
    return_val = {}
    op_ok = False
    op_msg = "Failed to get network information."
    op_config = ConfigParser.ConfigParser()
    network_info = {}
    
    try:
        with open(info_path) as f:
            op_config.readfp(f)
            ip = op_config.get('network', 'ip')
            gateway = op_config.get('network', 'gateway')
            mask = op_config.get('network', 'mask')
            dns1 = op_config.get('network', 'dns1')
            dns2 = op_config.get('network', 'dns2')
    
        network_info = {
            'ip': ip,
            'gateway': gateway,
            'mask': mask,
            'dns1': dns1,
            'dns2': dns2
        }

        op_ok = True
        op_msg = "Succeeded to get the network information."

    except IOError as e:
        op_ok = False
        op_msg = "Failed to access %s" % info_path
        log.error(op_msg)
    
    except Exception as e:
        op_ok = False
        log.error(str(e))

    finally:
        return_val = {
            'result': op_ok,
            'msg': op_msg,
            'data': network_info
        }
    
    return json.dumps(return_val)

def apply_network(ip, gateway, mask, dns1, dns2=None):
    """
    Set current storage gateway box network settings. 
    Should also trigger restart network service action.
    
    @type ip: string
    @param ip: IP address.
    @type gateway: string
    @param gateway: Network gateway.
    @type mask: string
    @param mask: Network mask.
    @type dns1: string
    @param dns1: Primary DNS.
    @type dns2: string
    @param dns2: Secondary DNS.
    @rtype: JSON object
    @return: Result of applying network.
    
        - result: Function call result
        - msg: Explanation of result
        - data: JSON object. Always return empty.
    """
    
    log.debug("apply_network start")

    ini_path = "/etc/delta/Gateway.ini"
    op_ok = False
    op_msg = "Failed to apply network configuration."
    op_config = ConfigParser.ConfigParser()

    try:
        with open(ini_path) as f:
            op_config.readfp(f)
    
            if ip == "" or ip == None:
                ip = op_config.get('network', 'ip')
            if gateway == "" or gateway == None:
                gateway = op_config.get('network', 'gateway')
            if mask == "" or mask == None:
                mask = op_config.get('network', 'mask')
            if dns1 == "" or dns1 == None:
                dns1 = op_config.get('network', 'dns1')
            if dns2 == "" or dns2 == None:
                dns2 = op_config.get('network', 'dns2')

        op_ok = True

    except IOError as e:
        op_ok = False
        log.error("Failed to access %s" % ini_path)

    except Exception as e:
        op_ok = False
        log.error(str(e))

    finally:
        if not op_ok:
            return_val = {
            'result': op_ok,
            'msg': op_msg,
            'data': {}
            }

            return json.dumps(return_val)
    
    try:
        if not _storeNetworkInfo(ini_path, ip, gateway, mask, dns1, dns2):
    
            return_val = {
                'result': False,
                'msg': "Failed to store the network information",
                'data': {}
            }
        
            return json.dumps(return_val)
        
        if _setInterfaces(ip, gateway, mask, dns1, dns2, ini_path):
            try:
                cmd = "sudo /etc/init.d/networking restart"
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                po.wait()

            except:
                op_ok = False
                log.warning("Starting SAVEBOX networking failed.")
            else:
                if os.system("sudo /etc/init.d/networking restart") == 0:
                    op_ok = True
                    op_msg = "Succeeded to apply the network configuration."
                    log.debug("Starting SAVEBOX networking was successful.")
                else:
                    op_ok = False
                    log.warning("Starting SAVEBOX networking failed.")
        else:
            op_ok = False
            log.error(op_msg)

    except:
        pass
    
    return_val = {
    'result': op_ok,
    'msg': op_msg,
    'data': {}
    }

    log.debug("apply_network end")
    return json.dumps(return_val)

def _storeNetworkInfo(ini_path, ip, gateway, mask, dns1, dns2=None):
    """
    Store network info to /etc/delta/network.info.
    
    @type ini_path: string
    @param ini_path: Path to ini file.
    @type ip: string
    @param ip: IP address.
    @type gateway: string
    @param gateway: Network gateway.
    @type mask: string
    @param mask: Network mask.
    @type dns1: string
    @param dns1: Primary DNS.
    @type dns2: string
    @param dns2: Secondary DNS.
    @rtype: boolean
    @return: Result of storing network info to ini file.
    """
    
    op_ok = False
    op_config = ConfigParser.ConfigParser()
    info_path = "/etc/delta/network.info"

    try:
        with open(ini_path) as f:
            op_config.readfp(f)
            op_config.set('network', 'ip', ip)
            op_config.set('network', 'gateway', gateway)
            op_config.set('network', 'mask', mask)
            op_config.set('network', 'dns1', dns1)
    
            if dns2 != None:
                op_config.set('network', 'dns2', dns2)
    
        with open(info_path, "wb") as f:
            op_config.write(f)
    
        os.system('sudo chown www-data:www-data %s' % info_path)
    
        op_ok = True
        log.debug("Succeeded to store the network information.")

    except IOError as e:
        op_ok = False
        log.error("Failed to store the network information in %s." % info_path)

    except Exception as e:
        op_ok = False
        log.error(str(e))

    finally:
        return op_ok

def _setInterfaces(ip, gateway, mask, dns1, dns2, ini_path):
    """
    Change the setting of /etc/network/interfaces.
    
    @type ip: string
    @param ip: IP address.
    @type gateway: string
    @param gateway: Network gateway.
    @type mask: string
    @param mask: Network mask.
    @type dns1: string
    @param dns1: Primary DNS.
    @type dns2: string
    @param dns2: Secondary DNS.
    @type ini_path: string
    @param ini_path: Ini file to get fixed IP and network mask (used for eth0 currently).
    @rtype: boolean
    @return: True if successfully applied the net setting. Otherwise, false.
    """
    
    interface_path = "/etc/network/interfaces"
    interface_path_temp = "/etc/delta/network_interfaces"
    op_ok = False
    op_config = ConfigParser.ConfigParser()

    try:
        with open(ini_path) as f:
            op_config.readfp(f)

        fixedIp = op_config.get('network', 'fixedIp')
        fixedMask = op_config.get('network', 'fixedMask')
        op_ok = True
        log.debug("Succeeded to get the fixed network information.")

    except IOError as e:
        op_ok = False
        log.error("Failed to access %s" % ini_path)
        return op_ok

    except Exception as e:
        op_ok = False
        log.error(str(e))
        return op_ok

    if os.path.exists(interface_path):
        os.system("sudo cp -p %s %s" % (interface_path, interface_path + "_backup"))
    else:
        os.system("sudo touch %s" % interface_path)
        log.warning("File does not exist: %s" % interface_path)

    try:
        with open(interface_path_temp, "w") as f:
            f.write("auto lo\niface lo inet loopback\n")
            f.write("\nauto eth0\niface eth0 inet static")
            f.write("\naddress %s" % fixedIp)
            f.write("\nnetmask %s\n" % fixedMask)
            f.write("\nauto eth1\niface eth1 inet static")
            f.write("\naddress %s" % ip)
            f.write("\nnetmask %s" % mask)
            f.write("\ngateway %s" % gateway)
            # wthung, 2012/7/30
            # for ubuntu 12.04, move dns setting to here
            f.write("\ndns-nameservers %s %s" % (dns1, dns2))
        os.system('sudo cp %s %s' % (interface_path_temp, interface_path))

        op_ok = True
        log.debug("Succeeded to set the network configuration")

    except IOError as e:
        op_ok = False
        log.error("Failed to access %s." % interface_path)

        if os.path.exists(interface_path + "_backup"):
            if os.system("sudo cp -p %s %s" % (interface_path + "_backup", interface_path)) != 0:
                log.warning("Failed to recover %s" % interface_path)
            else:
                log.debug("Succeeded to recover %s" % interface_path)

    except Exception as e:
        op_ok = False
        log.error(str(e))

    finally:
        return op_ok

# wthung, 2012/10/16
# content of resolv.conf is auto-generated. no need to modify it
#def _setNameserver(dns1, dns2=None):
#    """
#    Set the domain name server.
#    
#    @type dns1: string
#    @param dns1: Primiary DNS.
#    @type dns2: string
#    @param dns2: Secondary DNS.
#    @rtype: boolean
#    @return: True if successfully applied the DNS. Otherwise, false.
#    """
#    
#    nameserver_path = "/etc/resolv.conf"
#    nameserver_path_temp = "/etc/delta/temp_resolv.conf"
#    op_ok = False
#
#    if os.system("sudo cp -p %s %s" % (nameserver_path, nameserver_path + "_backup")) != 0:
#        os.system("sudo touch %s" % nameserver_path)
#        log.warning("File does not exist: %s" % nameserver_path)
#
#    try:
#        with open(nameserver_path_temp, "w") as f:
#            f.write("nameserver %s\n" % dns1)
#    
#            if dns2 != None:
#                f.write("nameserver %s\n" % dns2)
#
#        os.system('sudo cp %s %s' % (nameserver_path_temp, nameserver_path))
#
#        op_ok = True
#        log.debug("Succeeded to set the nameserver.")
#
#    except IOError as e:
#        op_ok = False
#        log.error("Failed to access %s." % nameserver_path)
#
#        if os.path.exists(nameserver_path + "_backup"):
#            if os.system("sudo cp -p %s %s" % (nameserver_path + "_backup", nameserver_path)) != 0:
#                log.warning("Failed to recover %s" % nameserver_path)
#            else:
#                log.debug("Succeeded to recover %s" % nameserver_path)
#
#    except Exception as e:
#        op_ok = False
#        log.error(str(e))
#
#    finally:
#        return op_ok

def get_scheduling_rules():        # by Yen
    """
    Get gateway scheduling rules.
    
    @rtype: JSON object
    @return: Gateway scheduling rules.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: A list to describe the scheduling rules.
    """
    
    # load config file 
    fpath = "/etc/delta/"
    fname = "gw_schedule.conf"
    try:
        with open(fpath + fname, 'r') as fh:
            fileReader = csv.reader(fh, delimiter=',', quotechar='"')
            schedule = []
            for row in fileReader:
                schedule.append(row)
            del schedule[0]   # remove header

    except:
        return_val = {
            'result': False,
            'msg': "Opening or writing failed.",
            'code': 0x8001,
            'data': []
        }
        return json.dumps(return_val)
    
    return_val = {
    'result': True,
    'msg': "Bandwidth throttling schedule was read successfully.",
    'code': 0xC,
    'data': schedule
    }
    
    return json.dumps(return_val)

def get_smb_user_list():
    """
    Get Samba user by reading /etc/samba/smb.conf.
    
    @rtype: JSON object
    @return: Result of getting Samba user and user info if success.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object.
            - accounts: Samba user account.
    """
    
    log.debug("get_smb_user_list")
    
    username = []
    op_ok = False
    op_msg = 'Smb account read failed unexpectedly.'

    try:
        parser = ConfigParser.SafeConfigParser() 
        parser.read(smb_conf_file)

        if parser.has_option("cloudgwshare", "valid users"):
            user = parser.get("cloudgwshare", "valid users")
            username = str(user).split(" ") 
        else:
            username.append(default_user_id)  # admin as the default user

        op_ok = True
        op_msg = 'Obtained smb account information'
        
    except ConfigParser.ParsingError:
        op_msg = smb_conf_file + ' is not readable.'
        log.error(op_msg)
        username.append(default_user_id)  # default
    
    return_val = {
                  'result' : op_ok,
                  'msg' : op_msg,
                  'data' : {'accounts' : username[0]}}

    log.debug("get_smb_user_list end")
        
    return json.dumps(return_val)

@common.timeout(RUN_CMD_TIMEOUT)
def _chSmbPasswd(username, password):
    """
    Change Samba password by calling change_smb_pwd.sh.
    
    @type username: string
    @param username: Samba user name.
    @type password: string
    @param password: Samba user password.
    @rtype: integer
    @return: 0 for success. Otherwise, nonzero value.
    """
    
    cmd = ["sudo", "sh", CMD_CH_SMB_PWD, username, password]

    proc = subprocess.Popen(cmd,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
            
    results = proc.stdout.read()
    ret_val = proc.wait()  # 0 : success

    log.debug("change smb val: %d, message %s" % (ret_val, results))
    if ret_val != 0:
        log.error("%s" % results)

    return ret_val

def set_smb_user_list(username, password):
    """
    Update username to /etc/samba/smb.conf and call smbpasswd to set password
    
    @type username: string
    @param username: Samba user name.
    @type password: string
    @param password: Samba user password.
    @rtype: JSON object
    @return: Result of setting Samba user.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    return_val = {
                  'result' : False,
                  'msg' : 'set Smb account failed unexpectedly.',
                  'data' : {} }

    log.debug("set_smb_user_list starts")

    # currently, only admin can use smb
    if str(username).lower() != default_user_id:
        return_val['msg'] = 'invalid user. Only accept ' + default_user_id
        return json.dumps(return_val)

    # ok, set the password
    # notice that only a " " is required btw tokens
     
    try:
        ret = _chSmbPasswd(username=username, password=password)
        if ret != 0:
            return_val['msg'] = 'Failed to change smb passwd'
            return json.dumps(return_val)

    except common.TimeoutError:
        log.error("set_smb_user_list timeout")
        return_val['msg'] = 'Timeout for changing passwd.'
        return json.dumps(return_val)

    #Resetting smb service
    smb_return_val = json.loads(restart_smb_service())
    
    if not smb_return_val['result']:
        return_val['msg'] = 'Error in restarting smb service.'
        return json.dumps(return_val) 
    
    # good. all set
    return_val['result'] = True
    return_val['msg'] = 'Success to set smb account and passwd'

    log.debug("set_smb_user_list end")
    return json.dumps(return_val)

def get_nfs_access_ip_list():
    """
    Get IP addresses allowed to access NFS.
    
    List was read from /etc/hosts.allow.
    
    @rtype: JSON object
    @return: Result of allowed IP addresses.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object.
            - array_of_ip: Allowed IP addresses.
    """

    return_val = {
                  'result' : False,
                  'msg' : 'get NFS access ip list failed unexpectedly.',
                  'data' : { "array_of_ip" : [] } }

    log.debug("get_nfs_access_ip_list starts")

    try:
        with open(nfs_hosts_allow_file, 'r') as fh:
            for line in fh:
                # skip comment lines and empty lines
                if str(line).startswith("#") or str(line).strip() == None: 
                    continue

                # accepted format:
                # portmap mountd nfsd statd lockd rquotad : 172.16.229.112 172.16.229.136

                arr = str(line).strip().split(":")

                # format error
                if len(arr) < 2:
                    log.error(str(nfs_hosts_allow_file) + " format error")
          
                    return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                    return json.dumps(return_val)

                # got good format
                # key = services allowed, val = ip lists
                #services = str(arr[0]).strip()
                iplist = arr[1]
                ips = iplist.strip().split(", ")

                # Jiahong: Hiding the first two ips in the list: 127.0.0.1 and 127.0.0.2
                if len(ips) < 2:
                    log.error(str(nfs_hosts_allow_file) + " format error")

                    return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                    return json.dumps(return_val)

                return_val['result'] = True
                return_val['msg'] = "Get ip list success"
                return_val['data']["array_of_ip"] = ips[2:]
    except :
        log.error("cannot parse " + str(nfs_hosts_allow_file))
        return_val['msg'] = "cannot parse " + str(nfs_hosts_allow_file)
    
    log.debug("get_nfs_access_ip_list end")
    return json.dumps(return_val)
    

def set_compression(switch):
    """
    Set status of compression switch.
    Will regenerate S3QL configuration file by calling _createS3qlConf().
    
    @type switch: boolean
    @param switch: Compression status.
    @rtype: JSON object
    @return: Compress switch.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    log.debug("set_compression start")
    op_ok = False
    op_msg = ''

    try:
        config = getGatewayConfig()
    
        if switch:
            config.set("s3ql", "compress", "true")
        else:
            config.set("s3ql", "compress", "false")
    
        storage_url = getStorageUrl()
        if storage_url is None:
            raise Exception("Failed to get storage url")
        
        account = _get_storage_account()
        if not account:
            raise Exception("Failed to get storage account")
    
        with open('/etc/delta/Gateway.ini', 'wb') as op_fh:
            config.write(op_fh)
        
    
        if  _createS3qlConf(storage_url, account) != 0:
            raise Exception("Failed to create new s3ql config")
    
        op_ok = True
        op_msg = "Succeeded to set_compression"

    except IOError as e:
        op_msg = str(e)
    except GatewayConfError as e:
        op_msg = str(e)
    except Exception as e:
        op_msg = str(e)
    finally:
        if not op_ok:
            log.error(op_msg)

    return_val = {'result' : op_ok,
                  'msg'    : op_msg,
                  'data'   : {}}
    
    log.debug("set_compression end")
    return json.dumps(return_val)

def set_nfs_access_ip_list(array_of_ip):
    """
    Update new IP addresses allowed to access NFS.
    
    Update to /etc/hosts.allow.
    The original IP list will be updated to the new IP list.
    Will restart NFS after completion.
    
    @type array_of_ip: list
    @param array_of_ip: New IP address allowed to access.
    @rtype: JSON object
    @return: Result of setting allowed IP addresses.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """

    return_val = {
                  'result' : False,
                  'msg' : 'get NFS access ip list failed unexpectedly.',
                  'data' : {} }

    log.debug("set_nfs_access_ip_list starts")

    try:
        # try to get services allowed
        with open(nfs_hosts_allow_file, 'r') as fh:
            for line in fh:
                # skip comment lines and empty lines
                if str(line).startswith("#") or str(line).strip() == None: 
                    continue
            
                arr = str(line).strip().split(":")

                # format error
                if len(arr) < 2:
                    log.error(str(nfs_hosts_allow_file) + " format error")
          
                    return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                    return json.dumps(return_val)

                # got good format
                services = str(arr[0]).strip()

                iplist = arr[1]
                ips = iplist.strip().split(", ")

                if len(ips) < 2:
                    log.error(str(nfs_hosts_allow_file) + " format error")

                    return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                    return json.dumps(return_val)

                # Jiahong: We need to keep the first two ips in the list
                fixed_ips = ips[0:2]
                full_ip_list = fixed_ips + array_of_ip

    except :
        log.error("cannot parse " + str(nfs_hosts_allow_file))
          
        return_val['msg'] = "cannot parse " + str(nfs_hosts_allow_file)

    # finally, updating the file
    nfs_hosts_allow_file_temp = '/etc/delta/hosts_allows_temp'
    try:
        ofile = open(nfs_hosts_allow_file_temp, 'w')
        output = services + " : " + ", ".join(full_ip_list) + "\n"
        ofile.write(output)
        ofile.close()
        os.system('sudo cp %s %s' % (nfs_hosts_allow_file_temp, nfs_hosts_allow_file))
        # yen, 2012/10/09. Remove outdated config files
        # save config to cloud
        api_restore_conf.save_gateway_configuration()

        return_val['result'] = True
        return_val['msg'] = "Update ip list successfully"

    except:
        log.error("cannot write to " + str(nfs_hosts_allow_file))
        return_val['msg'] = "cannot write to " + str(nfs_hosts_allow_file)

    try:
        #Resetting nfs service
        nfs_return_val = json.loads(restart_nfs_service())
    
        if not nfs_return_val['result']:
            return_val['result'] = False
            return_val['msg'] = 'Error in restarting NFS service.'
            return json.dumps(return_val)
    except:
        pass
        
    log.debug("set_nfs_access_ip_list end")
    
    return json.dumps(return_val)


# example schedule: [ [1,0,24,512],[2,0,24,1024], ... ]
def apply_scheduling_rules(schedule):        # by Yen
    """
    Set gateway scheduling rules.
    Will run /etc/cron.hourly/hourly_run_this after saving configuration file.
    
    @type schedule: list
    @param schedule: Scheduling rules.
    @rtype: JSON object
    @return: Result of setting scheduling rules.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    # write settings to gateway_throttling.cfg
    fpath = "/etc/delta/"
    fname = "gw_schedule.conf"
    try:
        with open(fpath + fname, "w") as fh:
            fptr = csv.writer(fh)
            header = ["Day", "Start_Hour", "End_Hour", "Bandwidth (in kB/s)"]
            fptr.writerow(header)
            for row in schedule:
                fptr.writerow(row)
        # apply settings
        os.system("sudo /etc/cron.hourly/hourly_run_this")
        return_val = {
            'result': True,
            'msg': "Rules of bandwidth schedule were saved successfully.",
            'code': 0x1,
            'data': {}
        }
        # yen, 2012/10/09.
        # save config to cloud
        api_restore_conf.save_gateway_configuration()

    except:
        return_val = {
            'result': False,
            'msg': "Opening or writing failed.",
            'code': 0x8001,
            'data': []
        }
        return json.dumps(return_val)
    
    return json.dumps(return_val)
    

def stop_upload_sync():        # by Yen
    """
    Turn off S3QL cache uploading.
    
    @rtype: JSON object
    @return: Result of setting c.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    # generate a new rule set and apply it to the gateway
    schedule = []
    for ii in range(1, 8):
        schedule.append([ii, 0, 24, 0])
    
    try:
        apply_scheduling_rules(schedule)
    except:
        return_val = {
            'result': False,
            'msg': "Turn off cache uploading has failed.",
            'data': {}
        }
        return json.dumps(return_val)
    
    return_val = {
    'result': True,
    'msg': "Cache upload has turned off.",
    'data': {}
    }
    return json.dumps(return_val)

def force_upload_sync(bw):        # by Yen
    """
    Turn on S3QL cache uploading and set bandwidth for it.
    
    @type bw: integer
    @param bw: Uploading bandwidth.
    @rtype: JSON object
    @return: Result of setting cache uploading on.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object. Always return empty.
    """
    
    if (bw < 256):
        return_val = {
            'result': False,
            'msg': "Uploading bandwidth has to be larger than 256KB/s.",
            'data': {}
    }
    return json.dumps(return_val)

    # generate a new rule set and apply it to the gateway
    schedule = []
    for ii in range(1, 8):
        schedule.append([ii, 0, 24, bw])
    
    try:
        apply_scheduling_rules(schedule)
    except:
        return_val = {
            'result': False,
            'msg': "Turn on cache uploading has failed.",
            'data': {}
        }
        return json.dumps(return_val)
    
    return_val = {
    'result': True,
    'msg': "Cache upload is turned on.",
    'data': {}
    }
    return json.dumps(return_val)

################################################################################
def month_number(monthabbr):
    """
    Return the month number for monthabbr.
    
    e.g. "Jan" -> 1.
    
    @type monthabbr: string
    @param monthabbr: Abbrevation of month.
    @rtype: integer
    @return: The numeric expression of the month.
    """

    MONTHS = ['',
              'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
              'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
              ]

    index = MONTHS.index(monthabbr)
    return index

def classify_logs(logs, keyword_filter=KEYWORD_FILTER):
    """
    Give a log message and a keyword filter {key=category, val = [keyword]}.
    Find out which category the log belongs to. 
    Assume that keywords are not overlaped.
    
    @type log: string
    @param log: Input log message to be classified.
    @type keyword_filter: dictionary
    @param keyword_filter: Keyword filter.
    @rtype: string
    @return: The category name of input log message.
    """

    for category in sorted(keyword_filter.keys()):
        for keyword in keyword_filter[category]:
            if re.search(keyword, logs):
                return category
            else:
                pass

    return None

def parse_log(type, log_cnt):
    """
    Parse a log line to log_entry data structure.
    Different types require different parser.
    If a log line doesn't match the pattern, it return None.
    
    #type = syslog
    #log_cnt = "May 10 13:43:46 ubuntu dhclient: bound to 172.16.229.78 -- renewal in 277 seconds."

    #type = mount | fsck # i.e.,g from s3ql
    #log_cnt = "2012-05-07 20:22:22.649 [1666] MainThread: [mount] FUSE main loop terminated."
    
    @type type: string
    @param type: Log type.
    @type log_cnt: Log content.
    @rtype: dictionary
    @return: A log_entry structure if successfuly parsed. Otherwise, None will be returned.
    """

    log_entry = {
                 "category" : "",
                 "timestamp" : "",
                 "msg" : ""
                 }

    pat = LOG_PARSER[type]

    m = pat.match(log_cnt)
    if m == None:
        return None 

    minute = int(m.group('minute'))
    hour = int(m.group('hour'))
    day = int(m.group('day'))
    second = int(m.group('second'))

    if len(m.group('month')) == 2:
        month = int(m.group('month'))
    else:
        month = month_number(m.group('month'))

    try:
        # syslog has't year info, try to fetch group("year") will cause exception
        year = int(m.group('year'))
    except:
        # any exception, using this year instead
        now = datetime.utcnow()
        year = now.year

    msg = m.group('message')

    if msg == None:  # skip empty log
        return None

    try:
        timestamp = datetime(year, month, day, hour, minute, second)  # timestamp
    except Exception:
        return None

    category = classify_logs(msg, KEYWORD_FILTER)
    if category == None:  # skip invalid log
        return None
    
    log_entry["category"] = category
    log_entry["timestamp"] = str(timestamp)  # str(timestamp.now()) # don't include ms
    log_entry["msg"] = msg[LOG_LEVEL_PREFIX_LEN:]
    return log_entry

##########################
def read_logs(logfiles_dict, offset, num_lines):
    """
    Read all files in logfiles_dict, 
    the log will be reversed since new log line is appended to the tail
    and then, each log is parsed into log_entry dict.
    
    The offset = 0 means that the latest log will be selected.
    num_lines means that how many lines of logs will be returned, 
    set to "None" for selecting all logs.
    
    @type logfiles_dict: dictionary
    @param logfiles_dict: Logs to be read.
    @type offset: integer
    @param offset: File offset.
    @type num_lines: integer
    @param num_lines: Max number of lines to be read.
    @rtype: integer
    @return: Count of read log.
    """

    ret_log_cnt = {}

    for logtype in logfiles_dict.keys():
        ret_log_cnt[logtype] = []

        log_buf = []

        try:
            log_buf = [line.strip() for line in open(logfiles_dict[logtype])]
            log_buf.reverse()

            # get the log file content
            #ret_log_cnt[type] = log_buf[ offset : offset + num_lines]

            # parse the log file line by line
            nums = NUM_LOG_LINES
            if num_lines == None:
                nums = len(log_buf) - offset  # all
            else:
                nums = num_lines

            for alog in log_buf[offset : offset + nums]:
                log_entry = parse_log(logtype, alog)
                if not log_entry == None:  # ignore invalid log line 
                    ret_log_cnt[logtype].append(log_entry)

        except :
            pass

            #if enable_log:
            #    log.info("cannot parse " + logfiles_dict[type])
            #ret_log_cnt[type] = ["None"]
            #print "cannot parse"

    return ret_log_cnt


##############################    
def _get_storage_capacity():
    """
    Read cloud storage and gateway usage and capacity.
    
    Format:
        - Directory entries:    601
        - Inodes:               603
        - Data blocks:          1012
        - Total data size:      12349.54 MB
        - After de-duplication: 7156.38 MB (57.95% of total)
        - After compression:    7082.76 MB (57.35% of total, 98.97% of de-duplicated)
        - Database size:        0.30 MB (uncompressed)
        (some values do not take into account not-yet-uploaded dirty blocks in cache)
        - Cache size: current: 7146.38 MB, max: 20000.00 MB
        - Cache entries: current: 1011, max: 250000
        - Dirty cache status: size: 0.00 MB, entries: 0
        - Cache uploading: Off
        - Dirty cache near full: False
    
    @rtype: JSON object
    @return: Cloud storage and gateway usage.
    
        - cloud_storage_usage: JSON object
            - cloud_data: Cloud data usage.
            - cloud_data_dedup: Cloud data usage after deduplication.
            - cloud_data_dedup_compress: Cloud data usage after deduplication and compression.
            - cloud_capacity: Cloud storage capacity.
        - gateway_cache_usage: JSON object
            - max_cache_size: Max cache size.
            - max_cache_entries: Max cache entries.
            - used_cache_size: Used cache size.
            - used_cache_entries: Used cache entries.
            - dirty_cache_size: Dirty cache size.
            - dirty_cache_entries: Dirty cache entries.
    """

    ret_usage = {
              "cloud_storage_usage" :  {
                                    "cloud_data" : 0,
                                    "cloud_data_dedup" : 0,
                                    "cloud_data_dedup_compress" : 0,
                                    "cloud_capacity": 0
                                  },
              "gateway_cache_usage"   :  {
                                    "max_cache_size" : 0,
                                    "max_cache_entries" : 0,
                                    "used_cache_size" : 0,
                                    "used_cache_entries" : 0,
                                    "dirty_cache_size" : 0,
                                    "dirty_cache_entries" : 0
                                   }
              }

    # Flush check & DirtyCache check
    try:
        cmd = "sudo python /usr/local/bin/s3qlstat /mnt/cloudgwfiles"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        results = po.stdout.read()
        po.wait()

        real_cloud_data = 0

        if po.returncode == 0:  # success
            '''
            very boring string parsing process.
            the format should be fixed. otherwise, won't work (no error checking)  
            '''
            # query size of s3ql metadata db
            # first compose the file name of db
            db_name = _get_s3ql_db_name()
            db_size = 0
            try:
                db_size = os.path.getsize('%s/%s' % (S3QL_CACHE_DIR, db_name))
            except os.error:
                log.warning('Failed to get size of S3QL metadata DB.')

            for line in results.split("\n"):
                if line.startswith("Total data size:"):
                    tokens = line.split(":")
                    val = tokens[1].replace("MB", "").strip()
                    real_cloud_data = float(val)
                    ret_usage["cloud_storage_usage"]["cloud_data"] = int(float(val) * 1024 ** 2)
                    # always count metadata size in
                    ret_usage["cloud_storage_usage"]["cloud_data"] += int(db_size / 10)

                if line.startswith("After de-duplication:"):
                    tokens = line.split(":")
                    size = str(tokens[1]).strip().split(" ")
                    val = size[0]
                    ret_usage["cloud_storage_usage"]["cloud_data_dedup"] = int(float(val) * 1024 ** 2) 

                if line.startswith("After compression:"):
                    tokens = line.split(":")
                    size = str(tokens[1]).strip().split(" ")
                    val = size[0]
                    ret_usage["cloud_storage_usage"]["cloud_data_dedup_compress"] = int(float(val) * 1024 ** 2) 

                if line.startswith("Cache size: current:"):
                    line = line.replace("," , ":")
                    tokens = line.split(":")
                    crt_size = tokens[2]
                    max_size = tokens[4]

                    crt_tokens = str(crt_size).strip().split(" ")
                    crt_val = crt_tokens[0]
                    ret_usage["gateway_cache_usage"]["used_cache_size"] = int(float(crt_val) * 1024 ** 2) 

                    max_tokens = str(max_size).strip().split(" ")
                    max_val = max_tokens[0]
                    ret_usage["gateway_cache_usage"]["max_cache_size"] = int(float(max_val) * 1024 ** 2) 

                if line.startswith("Cache entries: current:"):
                    line = line.replace("," , ":")
                    tokens = line.split(":")
                    crt_size = tokens[2]
                    max_size = tokens[4]

                    crt_tokens = str(crt_size).strip().split(" ")
                    crt_val = crt_tokens[0]
                    ret_usage["gateway_cache_usage"]["used_cache_entries"] = crt_val

                    max_tokens = str(max_size).strip().split(" ")
                    max_val = max_tokens[0]
                    ret_usage["gateway_cache_usage"]["max_cache_entries"] = max_val

                if line.startswith("Dirty cache status: size:"):
                    line = line.replace("," , ":")
                    tokens = line.split(":")
                    crt_size = tokens[2]
                    max_size = tokens[4]

                    crt_tokens = str(crt_size).strip().split(" ")
                    crt_val = crt_tokens[0]
                    real_cloud_data = real_cloud_data - float(crt_val)
                    ret_usage["gateway_cache_usage"]["dirty_cache_size"] = int(float(crt_val) * 1024 ** 2)
                    
                    # wthung, 2012/12/26, check if s3ql metadata is dirty
                    if results.find('Dirty metadata: True') != -1:
                        # add one tenth of db size to dirty cache size
                        ret_usage["gateway_cache_usage"]["dirty_cache_size"] += int(db_size / 10)

                    max_tokens = str(max_size).strip().split(" ")
                    max_val = max_tokens[0]
                    ret_usage["gateway_cache_usage"]["dirty_cache_entries"] = max_val
                
                # cloud capacity is the quota value
                if line.startswith("Quota:"):
                    tokens = line.split(":")
                    val = tokens[1].replace("MB", "").strip()
                    real_cloud_cap = float(val)
                    ret_usage["cloud_storage_usage"]["cloud_capacity"] = int(float(val) * 1024 ** 2)

    except Exception:
        if enable_log:
            log.error(CMD_CHK_STO_CACHE_STATE + " fail")

    return ret_usage

##############################
def calculate_net_speed(iface_name):
    """
    Call get_network_status to get current NIC status.
    Wait for 1 second, and call again. 
    Then calculate difference, i.e., up/down link.
    
    @type iface_name: string
    @param iface_name: Interface name. Ex: eth1.
    @rtype: dictionary
    @return: NIC network usage
    
        - uplink_usage: Network traffic of outcoming packets.
        - downlink_usage: Network traffic of incoming packets.
        - uplink_backend_usage: TBD
        - downlink_backend_usage: TBD
    """
    
    ret_val = {
               "uplink_usage" : 0 ,
               "downlink_usage" : 0,
               "uplink_backend_usage" : 0 ,
               "downlink_backend_usage" : 0
               }

    pre_status = get_network_status(iface_name)
    time.sleep(1)
    next_status = get_network_status(iface_name)

    try:
        ret_val["downlink_usage"] = int(int(next_status["recv_bytes"]) - int(pre_status["recv_bytes"])) / 1024  # KB 
        ret_val["uplink_usage"] = int(int(next_status["trans_bytes"]) - int(pre_status["trans_bytes"])) / 1024
    except:
        ret_val["downlink_usage"] = 0 
        ret_val["uplink_usage"] = 0
    
    return ret_val

def get_network_speed(iface_name):  # iface_name = eth1
    """
    Get network speed by reading file or call functions.
    The later case will consume at least one second to return.
    
    @type iface_name: string
    @param iface_name: Interface name. Ex: eth1.
    @rtype: dictionary
    @return: NIC network usage
    
        - uplink_usage: Network traffic of outcoming packets.
        - downlink_usage: Network traffic of incoming packets.
        - uplink_backend_usage: TBD
        - downlink_backend_usage: TBD
    """
    
    log.debug('get_network_speed start')
    
    # define net speed file
    netspeed_file = '/dev/shm/gw_netspeed'

    ret_val = {
               "uplink_usage" : 0 ,
               "downlink_usage" : 0,
               "uplink_backend_usage" : 0 ,
               "downlink_backend_usage" : 0
               }
    
    try:
        if os.path.exists(netspeed_file):
            # read net speed from file
            log.debug('Read net speed from file')
            with open(netspeed_file, 'r') as fh:
                ret_val = json.load(fh)
        else:
            # call functions directly. will consume at least one second
            log.debug('No net speed file existed. Consume one second to calculate')
            ret_val = calculate_net_speed(iface_name)

    except:
        ret_val["downlink_usage"] = 0 
        ret_val["uplink_usage"] = 0

    return ret_val

##############################
def get_network_status(iface_name):  # iface_name = eth1
    """
    Get network usage.
    So far, cannot get current uplink, downlink numbers,
    use file /proc/net/dev, i.e., current tx, rx instead of

    If the target iface cannot find, all find ifaces will be returned.
    
    @type iface_name: string
    @param iface_name: NIC interface name.
    @rtype: dictionary
    @return: Network usage.
    """
    
    ret_network = {}

    lines = open("/proc/net/dev", "r").readlines()

    columnLine = lines[1]
    _, receiveCols , transmitCols = columnLine.split("|")
    receiveCols = map(lambda a: "recv_" + a, receiveCols.split())
    transmitCols = map(lambda a: "trans_" + a, transmitCols.split())
    cols = receiveCols + transmitCols

    faces = {}
    for line in lines[2:]:
        if line.find(":") < 0:
            continue
        face, data = line.split(":")
        faceData = dict(zip(cols, data.split()))
        face = face.strip()
        faces[face] = faceData

    try:
        ret_network = faces[iface_name]
    except:
        ret_network = faces  # return all
        pass

    return ret_network

################################################################################
def get_gateway_status():
    """
    Report gateway status.
    
    @rtype: JSON object
    @return: Gateway status.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object.
            - error_log: Error log entries.
            - cloud_storage_usage: Cloud storage usage.
            - gateway_cache_usage: Gateway cache usage.
            - uplink_backend_usage: Network traffic from gateway to backend.
            - downlink_backend_usage: Network traffic from backend to gateway.
            - network: TBD
    """

    ret_val = {"result" : True,
               "msg" : "Getting SAVEBOX log and status was successful.",
               'code': 0x9,
               "data" : { "error_log" : [],
                       "cloud_storage_usage" : 0,
                       "gateway_cache_usage" : 0,
                       "uplink_usage" : 0,
                       "downlink_usage" : 0,
                       "uplink_backend_usage" : 0,
                       "downlink_backend_usage" : 0,
                       "network" : {}
                      }
            }

    if enable_log:
        log.debug("get_gateway_status")

    try:
        # get logs
        ret_val["data"]["error_log"] = read_logs(LOGFILES, 0 , NUM_LOG_LINES)

        # get usage
        usage = _get_storage_capacity()
        ret_val["data"]["cloud_storage_usage"] = usage["cloud_storage_usage"]
        ret_val["data"]["gateway_cache_usage"] = usage["gateway_cache_usage"]

    except:
        log.warning("Getting SAVEBOX status failed.")

    return json.dumps(ret_val)

################################################################################
def get_gateway_system_log(log_level, number_of_msg, category_mask):
    """
    Get sytem logs in gateway.
    
    Due to there are a few log src, e.g., mount, fsck, syslog, 
    the number_of_msg will apply to each category.
    So, 3 x number_of_msg of log may return for each type of log {info, err, war}.
    
    @type log_level: integer
    @param log_level: 0 to return error/warning/info logs.
        1 to return error/warning logs and 2 to return only error log.
    @type number_of_msg: integer
    @param number_of_msg: Max returned number of log entries.
    @type category_mask: string
    @param category_mask: 0 to hide logs from gateway/NFS/Samba category.
        1 to show them.
    @rtype: JSON object
    @return: Gateway system log.
    
        - result: Function call result.
        - msg: Explanation of result.
        - data: JSON object
            - error_log: Error logs.
            - warning_log: Warning logs.
            - info_log: Infomration logs.
    """

    ret_val = {"result" : True,
               "msg": "Getting SAVEBOX system log was successful.",
               "code": 0xA,
               "data": {"error_log": [],
                         "warning_log": [],
                         "info_log": []
                       }
               }
    # wthung, 2012/7/19
    # to suppress warning from pychecker of not using var
    if "0" in category_mask:
        pass
    
    try:
        logs = read_logs(LOGFILES, 0 , None)  # query all logs
    
        for level in SHOW_LOG_LEVEL[log_level]:
            for logfile in logs:  # mount, syslog, ...
                counter = 0  # for each info src, it has number_of_msg returned
    
                for alog in logs[logfile]:  # log entries
                    if counter >= number_of_msg:
                        break  # full, finish this src
                    try:
                        if alog["category"] == level:
                            ret_val["data"][level].append(alog)
                            counter = counter + 1
                        else:
                            pass
                    except:
                        pass  # any except, skip this line
    except:
        pass
    
    return json.dumps(ret_val)

# wthung
def get_last_backup_time():
    """
    Get the time that all data has been flushed to cloud (no dirty cache).
    
    @rtype: JSON object
    @return: Function call result and last backup time
            - result: function call result
            - time: last backup time
    """
    
    last_backup_time_file = '/root/.s3ql/gw_last_backup_time'
    result = False
    last_time = 0
    
    if not os.path.exists(last_backup_time_file):
        result = True
    else:
        try:
            with open(last_backup_time_file, 'r') as fh:
                time_str = fh.read()
            
            last_time = int(float(time_str))
            result = True
        except Exception as e:
            log.error('Failed to open %s for reading last backup time: %s' % (last_backup_time_file, str(e)))
            last_time = -1
    
    return_val = {'result': result,
                  'time': last_time}
    
    return json.dumps(return_val)

#######################################################


if __name__ == '__main__':
    pass
