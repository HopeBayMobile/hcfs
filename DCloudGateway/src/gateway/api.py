import sys
import csv
import json
import os
import ConfigParser
import common
import subprocess
import time
import re
from datetime import datetime
from gateway import snapshot

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


def getStorageUrl():
    """
    Get storage URL from /root/.s3ql/authinfo2.

    @rtype: string
    @return: Storage URL or None if failed.

    """

    log.info("getStorageUrl start")
    storage_url = None

    try:
        config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            config.readfp(op_fh)

        section = "CloudStorageGateway"
        storage_url = config.get(section, 'storage-url').replace("swift://", "")
    except Exception as e:
        log.error("Failed to getStorageUrl for %s" % str(e))
    finally:
        log.info("getStorageUrl end")
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

    log.info("get_compression start")
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

        log.info("get_compression end")
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
    @return: True if s3ql is healthy.
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
                    if output.find("/mnt/cloudgwfiles") != -1 and output.find("/mnt/nfssamba") != -1:
                        return True
                    break
    except:
        pass
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
    """
    
    #log.info("get_indicators start")
    op_ok = False
    op_msg = 'Gateway indicators read failed unexpectedly.'
    return_val = {
          'result' : op_ok,
          'msg'    : op_msg,
          'data'   : {'network_ok' : False,
          'system_check' : False,
          'flush_inprogress' : False,
          'dirtycache_nearfull' : False,
          'HDD_ok' : False,
          'NFS_srv' : False,
          'SMB_srv' : False,
          'snapshot_in_progress' : False,
          'HTTP_proxy_srv' : False,
          'S3QL_ok': False }}

    try:
        op_network_ok = _check_network()
        op_system_check = _check_process_alive('fsck.s3ql')
        op_flush_inprogress = _check_flush()
        op_dirtycache_nearfull = _check_dirtycache()
        op_HDD_ok = _check_HDD()
        op_NFS_srv = _check_nfs_service()
        op_SMB_srv = _check_smb_service()
        op_snapshot_in_progress = _check_snapshot_in_progress()
        op_Proxy_srv = _check_process_alive('squid3')
        op_s3ql_ok = _check_s3ql()

        # Jiahong: will need op_s3ql_ok = True to restart nfs and samba
        if op_NFS_srv is False and _check_process_alive('mount.s3ql') is True:
            restart_nfs_service()
        if op_SMB_srv is False and op_s3ql_ok is True:
            restart_smb_service()

        op_ok = True
        op_msg = "Gateway indicators read successfully."
    
        return_val = {
              'result' : op_ok,
              'msg'    : op_msg,
              'data'   : {'network_ok' : op_network_ok,
              'system_check' : op_system_check,
              'flush_inprogress' : op_flush_inprogress,
              'dirtycache_nearfull' : op_dirtycache_nearfull,
              'HDD_ok' : op_HDD_ok,
              'NFS_srv' : op_NFS_srv,
              'SMB_srv' : op_SMB_srv,
              'snapshot_in_progress' : op_snapshot_in_progress,
              'HTTP_proxy_srv' : op_Proxy_srv,
              'S3QL_ok': op_s3ql_ok}}
    except Exception as Err:
        log.info("Unable to get indicators")
        log.info("msg: %s" % str(Err))
        return return_val
    
    #log.info("get_indicators end successfully")
    return return_val

# by Rice
# modified by wthung, 2012/6/25
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
            - uplink_usage: Network traffic going from gateway.
            - downlink_usage: Network traffic coming to gateway.
    """

    #log.info("get_gateway_indicators start")
    op_ok = False
    op_msg = 'Gateway indicators read failed unexpectedly.'

    # Note: indicators and net speed are acuquired from different location
    #       don't mess them up
    return_val = {
          'result' : op_ok,
          'msg'    : op_msg,
          'data'   : {'network_ok' : False,
          'system_check' : False,
          'flush_inprogress' : False,
          'dirtycache_nearfull' : False,
          'HDD_ok' : False,
          'NFS_srv' : False,
          'SMB_srv' : False,
          'snapshot_in_progress' : False,
          'HTTP_proxy_srv' : False,
          'S3QL_ok': False}}
    return_val2 = {
          'uplink_usage' : 0,
          'downlink_usage' : 0}


    # test, for fast UI integration
    #return json.dumps(return_val)
    
    # indicator file
    indic_file = '/dev/shm/gw_indicator'
            
    try:
        # check if indicator file is exist
        if os.path.exists(indic_file):
            # read indicator file as result
            # deserialize json object from file
            #log.info('%s is existed. Try to get indicator from it' % indic_file)
            with open(indic_file) as fh:
                #return json.dumps(json.load(fh))
                return_val = json.load(fh)
        else:
            # invoke regular function calls
            log.info('No indicator file existed. Try to spend some time to get it')
            return_val = get_indicators()

        # below call already checks netspeed indic file
        return_val2 = get_network_speed(MONITOR_IFACE)
           
    except Exception as Err:
        log.info("msg: %s" % str(Err))
        return_val['data'].update(return_val2)
        return json.dumps(return_val)

    #log.info("get_gateway_indicators end")
    return_val['data'].update(return_val2)
    return json.dumps(return_val)

# wthung, 2012/7/17, retire this function and replace by _check_process_alive
#def _check_http_proxy_service():
#    """
#    Check whether Squid3 is running.
#    
#    @rtype: boolean
#    @return: True if Squid3 (HTTP proxy) is alive. Otherwise false.
#    
#    """
#    
#    op_proxy_check = False
#    #log.info("[2] _check_http_proxy start")
#
#    try:
#        cmd = "sudo ps aux | grep squid3"
#        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
#        lines = po.stdout.readlines()
#        po.wait()
#
#        if po.returncode == 0:
#            if len(lines) > 2:
#                op_proxy_check = True
#        else:
#            log.info(output)
#    except:
#        pass
#
#    #log.info("[2] _check_http_proxy end")
#    return op_proxy_check


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
    log.info("_traceroute_backend start")
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
        log.info("_traceroute_backend end")
    return op_msg


# check network connection from gateway to storage by Rice
def _check_network():
    """
    Check network connection from gateway to storage.
    
    @rtype: boolean
    @return: True if network is alive. Otherwise false.
    
    """
    
    op_network_ok = False
    log.info("_check_network start")
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
                cmd = "sudo swift -A https://%s/auth/v1.0 -U %s -K %s stat" % (full_storage_url, op_user, op_pass)
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
                        if output.find("Bytes:") != -1:
                            op_network_ok = True
                        break
        else:
            log.info(output)

    except IOError as e:
            log.error('Unable to access /root/.s3ql/authinfo2.')
            log.error(str(e))
    except Exception as e:
            log.error('Unable to obtain storage url or login info.')
            log.error(str(e))

    finally:
        log.info("_check_network end")
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

# wthung, 2012/7/17, retire this function and replace by _check_process_alive
# check fsck.s3ql daemon by Rice
#def _check_system():
#    """
#    Check fsck.s3ql daemon.
#    
#    @rtype: boolean
#    @return: True if fsck.s3ql is alive. Otherwise false.
#    
#    """
#    
#    op_system_check = False
#    log.info("_check_system start")
#
#    try:
#        cmd = "sudo ps aux | grep fsck.s3ql"
#        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
#        lines = po.stdout.readlines()
#        po.wait()
#    
#        if po.returncode == 0:
#            if len(lines) > 2:
#                op_system_check = True
#        else:
#            log.info(output)
#    except:
#        pass
#
#    log.info("_check_system end")
#    return op_system_check

# flush check by Rice
def _check_flush():
    """
    Check if S3QL dirty cache flushing is in progress.
    
    @rtype: boolean
    @return: True if dirty cache flushing is in progress. Otherwise false.
    
    """
    
    op_flush_inprogress = False
    log.info("_check_flush start")
    
    try:
        cmd = "sudo python /usr/local/bin/s3qlstat /mnt/cloudgwfiles"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
    
        if po.returncode == 0:
            if output.find("Cache uploading: On") != -1:
                op_flush_inprogress = True
    
        else:
            log.info(output)
    except:
        pass
    
    log.info("_check_flush end")
    return op_flush_inprogress

# dirty cache check by Rice
def _check_dirtycache():
    """
    Check if S3QL dirty cache is near full.
    
    @rtype: boolean
    @return: True if dirty cache is near full. Otherwise false.
    
    """
    
    op_dirtycache_nearfull = False
    log.info("_check_dirtycache start")

    try:
        cmd = "sudo python /usr/local/bin/s3qlstat /mnt/cloudgwfiles"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
    
        if po.returncode == 0:
            if output.find("Dirty cache near full: True") != -1:
                            op_dirtycache_nearfull = True
        else:
            log.info(output)
    except:
        pass

    log.info("_check_dirtycache end")
    return op_dirtycache_nearfull

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
            log.info('[2] Some error occurred when getting serial number of %s' % disk)
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
    #log.info("_check_HDD start")

    try:
        all_disk = common.getAllDisks()
        nu_all_disk = len(all_disk)
        op_all_disk = 0
        
        # wthung, 2012/7/18
        # check if hdds number is 3. If not, report the serial number of alive hdd to log
        if nu_all_disk < 3:
            log.info('[0] Some disks were lost. Please check immediately')
            for disk in all_disk:
                disk_sn = _get_serial_number(disk)
                log.info('[0] Alive disk serial number: %s' % disk_sn)
            op_disk_num = False
    
        for i in all_disk:
            cmd = "sudo smartctl -a %s" % i
        
            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()

            if output.find("SMART overall-health self-assessment test result: PASSED") != -1:
                op_all_disk += 1 
            else:
                log.info("[0] %s (SN: %s) SMART test result: NOT PASSED" % (i, _get_serial_number(i)))
        
        if (op_all_disk == len(all_disk)) and op_disk_num:
            op_HDD_ok = True

    except:
        pass

    #log.info("_check_HDD end")
    return op_HDD_ok

# check nfs daemon by Rice
def _check_nfs_service():
    """
    Check NFS daemon.
    
    @rtype: boolean
    @return: True if NFS is alive. Otherwise false.
    
    """
    
    op_NFS_srv = True
    #log.error("error_check_nfs_service start")
    #log.warning("warning_check_nfs_service start")
    #log.info("info_check_nfs_service start")

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
                # restart_nfs_service()  # Moved this line to get_indicators()
        else:
            #print 'Checking NFS server returns nonzero value!'
            #log.info(output)
            pass

    except:
        pass

    log.info("_check_nfs_service end")
    return op_NFS_srv

# check samba daemon by Rice
def _check_smb_service():
    """
    Check Samba and NetBIOS daemon.
    
    @rtype: boolean
    @return: True if Samba and NetBIOS service are all alive. Otherwise false.
    """

    op_SMB_srv = False
    #log.info("_check_smb_service start")    

    try:
        cmd = "sudo service smbd status"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        # no matter smbd is running or not, command always returns 0
        if po.returncode == 0:
            if output.find("running") != -1:
                op_SMB_srv = True
            #else:
                # restart_smb_service()  # Moved to get_indicators()
        else:
            log.info(output)
            # restart_smb_service()  # Moved to get_indicators()

        # if samba service is running, go check netbios
        if op_SMB_srv:
            cmd2 = "sudo service nmbd status"
            po2 = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output2 = po2.stdout.read()
            po2.wait()

            if po2.returncode == 0:
                if output2.find("running") == -1:
                    op_SMB_srv = False
                    # restart_service("nmbd")  # Moved to get_indicators()
            else:
                log.info(output)
                # restart_service("nmbd")  # Moved to get_indicators()

    except:
        pass
    #log.info("_check_smb_service end")
    return op_SMB_srv

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
    
    log.info("get_storage_account start")

    op_ok = False
    op_msg = 'Storage account read failed unexpectedly.'
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
        op_msg = 'Obtained storage account information'

    except IOError as e:
        op_msg = 'Unable to access /root/.s3ql/authinfo2.'
        log.error(str(e))
    except Exception as e:
        op_msg = 'Unable to obtain storage url or login info.' 
        log.error(str(e))

    return_val = {'result' : op_ok,
             'msg' : op_msg,
             'data' : {'storage_url' : op_storage_url, 'account' : op_account}}

    log.info("get_storage_account end")
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
    
    log.info("apply_storage_account start")

    op_ok = False
    op_msg = 'Failed to apply storage accounts for unexpected errors.'

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
        
        op_ok = True
        op_msg = 'Succeeded to apply storage account'

    except IOError as e:
        op_msg = 'Failed to access /root/.s3ql/authinfo2'
        log.error(str(e))
    except Exception as e:
        log.error(str(e))

    return_val = {'result' : op_ok,
          'msg'    : op_msg,
          'data'   : {}}

    log.info("apply_storage_account end")
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
    
    log.info("apply_user_enc_key start")

    op_ok = False
    op_msg = 'Failed to change encryption keys for unexpected errors.'

    try:
        #Check if the new key is of valid format
        if not common.isValidEncKey(new_key):
            op_msg = "New encryption Key has to an alphanumeric string of length between 6~20"    
            raise Exception(op_msg)
    
        op_config = ConfigParser.ConfigParser()
        if not os.path.exists('/root/.s3ql/authinfo2'):
            op_msg = "Failed to find authinfo2"
            raise Exception(op_msg)

        with open('/root/.s3ql/authinfo2', 'rb') as op_fh:
            op_config.readfp(op_fh)

        section = "CloudStorageGateway"
        if not op_config.has_section(section):
            op_msg = "Section CloudStorageGateway is not found."
            raise Exception(op_msg)
    
        #TODO: deal with the case where the key stored in /root/.s3ql/authoinfo2 is Wrong
        key = op_config.get(section, 'bucket-passphrase')
        if key != old_key:
            op_msg = "The old_key is incorrect"
            raise Exception(op_msg)
    
        _umount()
    
        storage_url = op_config.get(section, 'storage-url')
        cmd = "sudo python /usr/local/bin/s3qladm --cachedir /root/.s3ql passphrase %s/gateway/delta" % (storage_url)
        po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (stdout, stderr) = po.communicate(new_key)
        if po.returncode != 0:
            if stdout.find("Wrong bucket passphrase") != -1:
                op_msg = "The key stored in /root/.s3ql/authoinfo2 is incorrect!"
            else:
                op_msg = "Failed to change enc_key for %s" % stdout
            raise Exception(op_msg)
        
        op_config.set(section, 'bucket-passphrase', new_key)
        with open('/root/.s3ql/authinfo2', 'wb') as op_fh:
            op_config.write(op_fh)
        
        op_ok = True
        op_msg = 'Succeeded to apply new user enc key'

    except IOError as e:
        op_msg = 'Failed to access /root/.s3ql/authinfo2'
        log.error(str(e))
    except UmountError as e:
        op_msg = "Failed to umount s3ql for %s" % str(e)
    except common.TimeoutError as e:
        op_msg = "Failed to umount s3ql in 10 minutes."
    except Exception as e:
        log.error(str(e))
    finally:
        log.info("apply_user_enc_key end")

        return_val = {'result' : op_ok,
              'msg'    : op_msg,
              'data'   : {}}

    return json.dumps(return_val)

def _createS3qlConf(storage_url):
    """
    Create S3QL configuration file and upstart script of gateway 
    by calling createS3qlconf.sh and createpregwconf.sh
    
    @type storage_url: string
    @param storage_url: Storage URL.
    @rtype: integer
    @return: 0 for success. Otherwise, nonzero value.
    
    """
    
    log.info("_createS3qlConf start")
    ret = 1
    try:
        config = getGatewayConfig()
        mountpoint = config.get("mountpoint", "dir")
        mountOpt = config.get("s3ql", "mountOpt")
        iface = config.get("network", "iface")
        compress = "lzma" if config.get("s3ql", "compress") == "true" else "none"
        mountOpt = mountOpt + " --compress %s" % compress 
    
        cmd = 'sudo sh %s/createS3qlconf.sh %s %s %s "%s"' % (DIR, iface, "swift://%s/gateway/delta" % storage_url, mountpoint, mountOpt)
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
        log.info("_createS3qlConf end")
    return ret
        

@common.timeout(180)
def _openContainter(storage_url, account, password):
    """
    Open container.
    
    Will raise BuildGWError if failed.
    
    @type storage_url: string
    @param storage_url: Storage URL.
    @type account: string
    @param account: Account name.
    @type password: string
    @param password: Account password.
    
    """
    
    log.info("_openContainer start")

    try:
        os.system("sudo touch gatewayContainer.txt")
        cmd = "sudo swift -A https://%s/auth/v1.0 -U %s -K %s upload gateway gatewayContainer.txt" % (storage_url, account, password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
    
        if po.returncode != 0:
            op_msg = "Failed to open container for" % output
            raise BuildGWError(op_msg)
        
        output = output.strip()
        if output != "gatewayContainer.txt":
            op_msg = "Failed to open container for %s" % output
            raise BuildGWError(op_msg)
        os.system("sudo rm gatewayContainer.txt")
    finally:
        log.info("_openContainer end")


@common.timeout(180)
def _mkfs(storage_url, key):
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
    
    log.info("[2] _mkfs start")
    
    # wthung, 2012/8/3
    # add a var to indicate an exiting filesys
    has_existing_filesys = False

    try:
        cmd = "sudo python /usr/local/bin/mkfs.s3ql --cachedir /root/.s3ql --authfile /root/.s3ql/authinfo2 --max-obj-size 2048 swift://%s/gateway/delta" % (storage_url)
        po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdout, stderr) = po.communicate(key)
        if po.returncode != 0:
            if stderr.find("existing file system!") == -1:
                op_msg = "Failed to mkfs for %s" % stderr
                raise BuildGWError(op_msg)
            else:
                log.info("[2] Found existing file system!")
                log.info("[2] Conducting forced file system check")
                has_existing_filesys = True
                
                cmd = "sudo python /usr/local/bin/fsck.s3ql --batch --force --authfile /root/.s3ql/authinfo2 --cachedir /root/.s3ql swift://%s/gateway/delta" % (storage_url)
                po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.read()
                po.wait()

                if po.returncode != 0:
                    log.error("[error] Error found during fsck (%s)" % output)
                else:
                    log.info("[2] fsck completed")
    # wthung, 2012/8/3
    # add except
    except Exception as e:
        log.info('[0] _mkfs error: %s' % str(e))

    finally:
        log.info("[2] _mkfs end")
        return has_existing_filesys


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
    
    log.info("[2] Gateway umounting")
    op_ok = False

    try:
        config = getGatewayConfig()
        mountpoint = config.get("mountpoint", "dir")
    
        if os.path.ismount(mountpoint):
            cmd = "sudo /etc/init.d/smbd stop"
            po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()
            if po.returncode != 0:
                raise UmountError(output)

            cmd = "sudo /etc/init.d/nmbd stop"
            po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()
            if po.returncode != 0:
                raise UmountError(output)
            
            # wthung, 2012/8/1
            # umount /mnt/nfssamba
            cmd = "sudo umount /mnt/nfssamba"
            po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()
            if po.returncode != 0:
                raise UmountError(output)

            cmd = "sudo /etc/init.d/nfs-kernel-server stop"
            po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()
            if po.returncode != 0:
                raise UmountError(output)

            cmd = "sudo python /usr/local/bin/umount.s3ql %s" % (mountpoint)
            po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()
            if po.returncode != 0:
                raise UmountError(output)
            
            op_ok = True
    except Exception as e:
        raise UmountError(str(e))
    
    finally:
        if op_ok == False:
            log.info("[0] Gateway umount error.")
        else:
            log.info("[2] Gateway umounted")


@common.timeout(360)
def _mount(storage_url):
    """
    Mount the S3QL file system.
    Will create S3QL configuration file and gateway upstart script before mounting.
        
    Will raise BuildGWError if failed.
    
    @type storage_url: string
    @param stroage_url: Storage URL.
    
    """
    
    log.info("[2] Gateway mounting")
    op_ok = False
    try:
        config = getGatewayConfig()
    
        mountpoint = config.get("mountpoint", "dir")
        mountOpt = config.get("s3ql", "mountOpt")
        compressOpt = "lzma" if config.get("s3ql", "compress") == "true" else "none"    
        mountOpt = mountOpt + " --compress %s" % compressOpt

        authfile = "/root/.s3ql/authinfo2"

        os.system("sudo mkdir -p %s" % mountpoint)

        if os.path.ismount(mountpoint):
            raise BuildGWError("A filesystem is mounted on %s" % mountpoint)

        if _createS3qlConf(storage_url) != 0:
            raise BuildGWError("Failed to create s3ql conf")

        #mount s3ql
        cmd = "sudo python /usr/local/bin/mount.s3ql %s --authfile %s --cachedir /root/.s3ql swift://%s/gateway/delta %s" % (mountOpt, authfile, storage_url, mountpoint)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            if output.find("Wrong bucket passphrase") != -1:
                raise EncKeyError("The input encryption key is wrong!")
            raise BuildGWError(output)

        #mkdir in the mountpoint for smb share
        cmd = "sudo mkdir -p %s/sambashare" % mountpoint
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            raise BuildGWError(output)

        #change the owner of samba share to default smb account
        cmd = "sudo chown superuser:superuser %s/sambashare" % mountpoint
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            raise BuildGWError(output)

        #mkdir in the mountpoint for nfs share
        cmd = "sudo mkdir -p %s/nfsshare" % mountpoint
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            raise BuildGWError(output)
        
        # wthung, 2012/7/30
        # create /mnt/nfssamba
        cmd = "sudo mkdir -p /mnt/nfssamba"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            raise BuildGWError(output)

        #change the owner of nfs share to nobody:nogroup
        cmd = "sudo chown nobody:nogroup %s/nfsshare" % mountpoint
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            raise BuildGWError(output)

        op_ok = True
    except GatewayConfError:
        raise
    except EncKeyError:
        raise
    except Exception as e:
        op_msg = "Failed to mount filesystem for %s" % str(e)
        log.error(str(e))
        raise BuildGWError(op_msg)

        if op_ok == False:
            log.info("[0] Gateway mount error.")
        else:
            log.info("[2] Gateway mounted")
    

@common.timeout(360)
def _restartServices():
    """
    Restart all gateway services.
        - Samba
        - NetBIOS
        - NFS
    
    Will raise BuildGWError if failed.
    
    """
    
    log.info("[2] Gateway restarting")
 
    #log.info("_restartServices start")
    try:
        config = getGatewayConfig()
    
        cmd = "sudo /etc/init.d/smbd restart"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            op_msg = "Failed to start samba service for %s." % output
            raise BuildGWError(op_msg)

        cmd = "sudo /etc/init.d/nmbd restart"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            op_msg = "Failed to start samba service for %s." % output
            raise BuildGWError(op_msg)

        cmd = "sudo /etc/init.d/nfs-kernel-server restart"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output = po.stdout.read()
        po.wait()
        if po.returncode != 0:
            op_msg = "Failed to start nfs service for %s." % output
            raise BuildGWError(op_msg)

    except GatewayConfError as e:
        raise e, None, sys.exc_info()[2]

    except Exception as e:
        op_msg = "Failed to restart smb&nfs services for %s" % str(e)
        log.error(str(e))
        raise BuildGWError(op_msg)
    finally:
        log.info("_restartServices start")

    log.info("[2] Gateway restarted")


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
    
    log.info("[2] Gateway building")
    #log.info("build_gateway start")

    op_ok = False
    op_msg = 'Failed to apply storage accounts for unexpected errors.'

    try:

        if not common.isValidEncKey(user_key):
            op_msg = "Encryption Key has to be an alphanumeric string of length between 6~20"    
            raise BuildGWError(op_msg)

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
        
        _openContainter(storage_url=url, account=account, password=password)
        has_filesys = _mkfs(storage_url=url, key=user_key)
        _mount(storage_url=url)
        
        # wthung, 2012/8/3
        # if a file system is existed, try to rebuild snapshot database
        if has_filesys:
            snapshot.rebuild_snapshot_database()
        
        # restart nfs and mount /mnt/nfssamba
        restart_service("nfs-kernel-server")
        os.system("sudo mount -t nfs 127.0.0.1:/mnt/cloudgwfiles/sambashare/ /mnt/nfssamba")
        
        set_smb_user_list(default_user_id, default_user_pwd)
        restart_service("smbd")
        restart_service("nmbd")
        
        log.info("setting upload speed")
        os.system("sudo /etc/cron.hourly/hourly_run_this")
        # we need to manually exec background task program,
        #   because it is originally launched by upstart 
        # launch background task program
        os.system("/usr/bin/python /etc/delta/gw_bktask.py")
     
        op_ok = True
        op_msg = 'Succeeded to build gateway'

    except common.TimeoutError:
        op_msg = "Build Gateway failed due to timeout" 
    except IOError as e:
        op_msg = 'Failed to access /root/.s3ql/authinfo2'
    except EncKeyError as e:
        op_msg = str(e)
    except BuildGWError as e:
        op_msg = str(e)
    except Exception as e:
        op_msg = str(e)
    finally:
        if not op_ok:
            log.error(op_msg)
            log.info("[0] Gateway building error. " + op_msg)
        else:
            log.info("[2] Gateway builded")
            
        return_val = {'result' : op_ok,
                      'msg'    : op_msg,
                      'data'   : {}}

        log.info("build_gateway end")
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
    
    log.info("[2] NFS service restarting")
    log.info("restart_nfs_service start")

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
        log.info("[0] NFS service restarting error")

    finally:
        return_val = {
            'result': op_ok,
            'msg': op_msg,
            'data': {}
        }
    
        log.info("restart_nfs_service end")
        log.info("[2] NFS service restarted")
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
    
    log.info("restart_smb_service start")
    log.info("[2] Samba service restarting")

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
            log.info("[2] Samba service restarted")

    except Exception as e:
        op_ok = False
        log.error(str(e))
        log.info("[0] Samba service restarting error")

    finally:
        return_val = {
            'result': op_ok,
            'msg': op_msg,
            'data': {}
        }
    
        log.info("restart_smb_service end")
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
    
    log.info("[2] %s service restarting" % svc_name)

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
            log.info("[2] %s service restarted successfully." % svc_name)
            op_msg = "Restarting %s service succeeded." % svc_name

    except Exception as e:
        op_ok = False
        log.error(str(e))
        log.info("[0] %s service restarting error" % svc_name)

    finally:
        return_val = {
            'result': op_ok,
            'msg': op_msg,
            'data': {}
        }
    
        log.info("[2] %s service restarted" % svc_name)
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
    log.info("[2] Gateway restarting")

    return_val = {'result': True,
                  'msg': "Succeeded to reset the gateway.",
                  'data': {}}
    
    try:
        pid = os.fork()
    
        if pid == 0:
            time.sleep(10)
            os.system("sudo reboot")
        else:
            log.info("[2] Gateway will restart after ten seconds")
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
    log.info("[2] Gateway shutdowning")
    
    return_val = {'result': True,
                  'msg': "Succeeded to shutdown the gateway.",
                  'data': {}}

    try:
        pid = os.fork()
    
        if pid == 0:
            time.sleep(10)
            os.system("sudo poweroff")
        else:
            log.info("[2] Gateway will shutdown after ten seconds")
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

    if po.returncode != 0:
        op_msg = "Test storage account failed for %s" % output
        raise TestStorageError(op_msg)

    if not common.isHttp200(output):
        op_msg = "Test storage account failed"
        raise TestStorageError(op_msg)

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
    
    log.info("test_storage_account start")

    op_ok = False
    op_msg = 'Test storage account failed for unexpected errors.'

    try:
        _test_storage_account(storage_url=storage_url, account=account, password=password)
        op_ok = True
        op_msg = 'Test storage account succeeded'
        
    except common.TimeoutError:
        op_msg = "Test storage account failed due to timeout" 
        log.error(op_msg)
    except TestStorageError as e:
        op_msg = str(e)
        log.error(op_msg)
    except Exception as e:
        log.error(str(e))

    #Jiahong: Insert traceroute info in the case of a failed test
    if op_ok is False:
        try:
            traceroute_info = _traceroute_backend(storage_url)
            log.error(traceroute_info)
            op_msg = op_msg + '\n' + traceroute_info
        except Exception as e:
            log.error('Error in traceroute:\n' + str(e))

    return_val = {'result' : op_ok,
                  'msg'    : op_msg,
                  'data'   : {}}
    
    log.info("test_storage_account end")
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
    
    log.info("get_network start")
    #log.info("[2] Gateway networking starting")

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
    
        log.info("get_network end")
        #log.info("[2] Gateway networking started")
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
    
    log.info("apply_network start")
    log.info("[2] Gateway networking starting")

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
        
        if _setInterfaces(ip, gateway, mask, dns1, dns2, ini_path) and _setNameserver(dns1, dns2):
            try:
                cmd = "sudo /etc/init.d/networking restart"
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                po.wait()

            except:
                op_ok = False
                log.info("[0] Gateway networking starting error")
            else:
                if os.system("sudo /etc/init.d/networking restart") == 0:
                    op_ok = True
                    op_msg = "Succeeded to apply the network configuration."
                    log.info("[2] Gateway networking started")
                else:
                    op_ok = False
                    log.info("[0] Gateway networking starting error")
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

    log.info("apply_network end")
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
        log.info("Succeeded to store the network information.")

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
        #fixedGateway = op_config.get('network', 'fixedGateway')
        op_ok = True
        log.info("Succeeded to get the fixed network information.")

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
            # fro ubuntu 12.04, move dns setting to here
            f.write("\ndns-nameservers %s %s" % (dns1, dns2))
        os.system('sudo cp %s %s' % (interface_path_temp, interface_path))

        op_ok = True
        log.info("Succeeded to set the network configuration")

    except IOError as e:
        op_ok = False
        log.error("Failed to access %s." % interface_path)

        if os.path.exists(interface_path + "_backup"):
            if os.system("sudo cp -p %s %s" % (interface_path + "_backup", interface_path)) != 0:
                log.warning("Failed to recover %s" % interface_path)
            else:
                log.info("Succeeded to recover %s" % interface_path)

    except Exception as e:
        op_ok = False
        log.error(str(e))

    finally:
        return op_ok

def _setNameserver(dns1, dns2=None):
    """
    Set the domain name server.
    
    @type dns1: string
    @param dns1: Primiary DNS.
    @type dns2: string
    @param dns2: Secondary DNS.
    @rtype: boolean
    @return: True if successfully applied the DNS. Otherwise, false.
    """
    
    nameserver_path = "/etc/resolv.conf"
    nameserver_path_temp = "/etc/delta/temp_resolv.conf"
    op_ok = False

    if os.system("sudo cp -p %s %s" % (nameserver_path, nameserver_path + "_backup")) != 0:
        os.system("sudo touch %s" % nameserver_path)
        log.warning("File does not exist: %s" % nameserver_path)

    try:
        with open(nameserver_path_temp, "w") as f:
            f.write("nameserver %s\n" % dns1)
    
            if dns2 != None:
                f.write("nameserver %s\n" % dns2)

        os.system('sudo cp %s %s' % (nameserver_path_temp, nameserver_path))

        op_ok = True
        log.info("Succeeded to set the nameserver.")

    except IOError as e:
        op_ok = False
        log.error("Failed to access %s." % nameserver_path)

        if os.path.exists(nameserver_path + "_backup"):
            if os.system("sudo cp -p %s %s" % (nameserver_path + "_backup", nameserver_path)) != 0:
                log.warning("Failed to recover %s" % nameserver_path)
            else:
                log.info("Succeeded to recover %s" % nameserver_path)

    except Exception as e:
        op_ok = False
        log.error(str(e))

    finally:
        return op_ok

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
            'msg': "Open " + fname + " failed.",
            'data': []
        }
        return json.dumps(return_val)
    
    return_val = {
    'result': True,
    'msg': "Bandwidth throttling schedule is read.",
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

    username = []

    log.info("get_smb_user_list")

    op_ok = False
    op_msg = 'Smb account read failed unexpectedly.'

    try:
        parser = ConfigParser.SafeConfigParser() 
        parser.read(smb_conf_file)

        if parser.has_option("cloudgwshare", "valid users"):
            user = parser.get("cloudgwshare", "valid users")
            username = str(user).split(" ") 
        else:
            #print "parser read fail"
            username.append(default_user_id)  # admin as the default user

        op_ok = True
        op_msg = 'Obtained smb account information'
        
    except ConfigParser.ParsingError:
        #print err
        op_msg = smb_conf_file + ' is not readable.'
        
        log.error(op_msg)
            
        username.append(default_user_id)  # default
        
        #print "file is not readable"
    
    return_val = {
                  'result' : op_ok,
                  'msg' : op_msg,
                  'data' : {'accounts' : username[0]}}

    log.info("get_smb_user_list end")
        
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

    log.info("change smb val: %d, message %s" % (ret_val, results))
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

    log.info("set_smb_user_list starts")

    # currently, only admin can use smb
    if str(username).lower() != default_user_id:
        return_val['msg'] = 'invalid user. Only accept ' + default_user_id
        return json.dumps(return_val)
    
    # get current user list
#    try:
#        current_users = get_smb_user_list ()
#        #load_userlist = json.loads(current_users)    
#        #username_arr = load_userlist["data"]["accounts"]
#        
#        #print username_arr
#    except:
#        log.error("set_smb_user_list fails")
#        return_val['msg'] = 'cannot read current user list.'
#        return json.dumps(return_val)
    
    # for new user, add the new user to linux, update smb.conf, and set password
    # TODO: impl.
    
    # admin must in the current user list
    #flag = False
#    for u in username_arr:
#        #print u
#        if u == default_user_id:
#            flag = True
    '''
    if flag == False: # should not happen
        log.info("set_smb_user_list fails")
        return_val['msg'] = 'invalid user, and not in current user list.'
        return json.dumps(return_val)
    ''' 
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

    log.info("set_smb_user_list end")

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

    log.info("get_nfs_access_ip_list starts")

    try:
        with open(nfs_hosts_allow_file, 'r') as fh:
            for line in fh:
                # skip comment lines and empty lines
                if str(line).startswith("#") or str(line).strip() == None: 
                    continue
            
                #print line

                # accepted format:
                # portmap mountd nfsd statd lockd rquotad : 172.16.229.112 172.16.229.136

                arr = str(line).strip().split(":")
                #print arr

                # format error
                if len(arr) < 2:
                    log.info(str(nfs_hosts_allow_file) + " format error")
          
                    return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                    return json.dumps(return_val)

                # got good format
                # key = services allowed, val = ip lists
                #services = str(arr[0]).strip()
                iplist = arr[1]
                ips = iplist.strip().split(", ")
            
                #print services
                #print ips
                # Jiahong: Hiding the first two ips in the list: 127.0.0.1 and 127.0.0.2
                if len(ips) < 2:
                    log.info(str(nfs_hosts_allow_file) + " format error")

                    return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                    return json.dumps(return_val)


                return_val['result'] = True
                return_val['msg'] = "Get ip list success"
                return_val['data']["array_of_ip"] = ips[2:]
            
                #return json.dumps(return_val)
            
    except :
        log.info("cannot parse " + str(nfs_hosts_allow_file))
          
        return_val['msg'] = "cannot parse " + str(nfs_hosts_allow_file)
        #return json.dumps(return_val)
    
    log.info("get_nfs_access_ip_list end")
        
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
    
    log.info("set_compression start")
    op_ok = False
    op_msg = ''
    #op_switch = True

    try:
        config = getGatewayConfig()
    
        if switch:
            config.set("s3ql", "compress", "true")
        else:
            config.set("s3ql", "compress", "false")
        # wthung, 2012/7/18
        # mark below code
#        else:
#            raise Exception("The input argument has to be True or False")
    
        storage_url = getStorageUrl()
        if storage_url is None:
            raise Exception("Failed to get storage url")
    
        with open('/etc/delta/Gateway.ini', 'wb') as op_fh:
            config.write(op_fh)
    
        if  _createS3qlConf(storage_url) != 0:
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
    
        log.info("set_compression end")
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

    log.info("set_nfs_access_ip_list starts")

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
                    log.info(str(nfs_hosts_allow_file) + " format error")
          
                    return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                    return json.dumps(return_val)

            # got good format
            # key = services allowed, val = ip lists
                services = str(arr[0]).strip()
                #iplist = arr[1]
                #ips = iplist.strip().split(", ") #

                iplist = arr[1]
                ips = iplist.strip().split(", ")

                #print services
                #print ips
                if len(ips) < 2:
                    log.info(str(nfs_hosts_allow_file) + " format error")

                    return_val['msg'] = str(nfs_hosts_allow_file) + " format error"
                    return json.dumps(return_val)

                # Jiahong: We need to keep the first two ips in the list
                fixed_ips = ips[0:2]
                full_ip_list = fixed_ips + array_of_ip

    except :
        log.info("cannot parse " + str(nfs_hosts_allow_file))
          
        return_val['msg'] = "cannot parse " + str(nfs_hosts_allow_file)
        #return json.dumps(return_val)

    # finally, updating the file
    nfs_hosts_allow_file_temp = '/etc/delta/hosts_allows_temp'
    try:
        ofile = open(nfs_hosts_allow_file_temp, 'w')
        output = services + " : " + ", ".join(full_ip_list) + "\n"
        ofile.write(output)
        ofile.close()
        os.system('sudo cp %s %s' % (nfs_hosts_allow_file_temp, nfs_hosts_allow_file))

        return_val['result'] = True
        return_val['msg'] = "Update ip list successfully"
    except:
        log.info("cannot write to " + str(nfs_hosts_allow_file))

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
        
    log.info("set_nfs_access_ip_list end")
    
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
            'msg': "Rules of bandwidth schedule are saved.",
            'data': {}
        }

    except:
        return_val = {
            'result': False,
            'msg': "Open " + fname + " to write failed.",
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
        #print "Please check whether s3qlctrl is installed."
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
        #print "Please check whether s3qlctrl is installed."
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
    #print logs
    for category in sorted(keyword_filter.keys()):

        for keyword in keyword_filter[category]:
            #print "in keyword = " + keyword
            if re.search(keyword, logs):
                #print "match"
                return category
            else:
                #print "mismatch"
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

    #print "in parsing log"
    #print type
    #print log_cnt

    log_entry = {
                 "category" : "",
                 "timestamp" : "",
                 "msg" : ""
                 }

    pat = LOG_PARSER[type]

    m = pat.match(log_cnt)
    if m == None:
        #print "Not found"
        return None

    #print "match"
    #print m.group()
    #return 

    minute = int(m.group('minute'))
    #print minute

    hour = int(m.group('hour'))
    #print hour

    day = int(m.group('day'))
    #print day

    second = int(m.group('second'))
    #print second

    if len(m.group('month')) == 2:
        month = int(m.group('month'))
    else:
        month = month_number(m.group('month'))
    #print month

    try:
        # syslog has't year info, try to fetch group("year") will cause exception
        year = int(m.group('year'))
    except:
        # any exception, using this year instead
        now = datetime.utcnow()
        year = now.year

    #print year

    msg = m.group('message')
    #print msg

    if msg == None:  # skip empty log
        return None
    #now = datetime.datetime.utcnow()

    try:
        timestamp = datetime(year, month, day, hour, minute, second)  # timestamp
    except Exception:
        #print "datatime error"
        #print Exception
        #print err
        return None

    #print "timestamp = "
    #print timestamp
    #print "msg = "
    #print msg
    
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

            #print log_buf
            #return {}

            # get the log file content
            #ret_log_cnt[type] = log_buf[ offset : offset + num_lines]

            # parse the log file line by line
            nums = NUM_LOG_LINES
            if num_lines == None:
                nums = len(log_buf) - offset  # all
            else:
                nums = num_lines

            for alog in log_buf[offset : offset + nums]:
                #print log
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
def storage_cache_usage():
    """
    Read cloud storage and gateway usage.
    
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
                                    "cloud_data_dedup_compress" : 0
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
        #print CMD_CHK_STO_CACHE_STATE
        #proc  = subprocess.Popen("sudo python /usr/local/bin/s3qlstat /mnt/cloudgwfiles", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        #args = str(CMD_CHK_STO_CACHE_STATE).split(" ")
        #proc = subprocess.Popen(args,
        #                        stdout=subprocess.PIPE,
        #                        stderr=subprocess.STDOUT)

        #results = proc.stdout.read()
        #print results

        #ret_val = proc.wait() # 0 : success
        #proc.kill()

        cmd = "sudo python /usr/local/bin/s3qlstat /mnt/cloudgwfiles"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        results = po.stdout.read()
        po.wait()

        #print ret_val
        real_cloud_data = 0

        if po.returncode == 0:  # success
            '''
            very boring string parsing process.
            the format should be fixed. otherwise, won't work (no error checking)  
            '''

            for line in results.split("\n"):
                if line.startswith("Total data size:"):
                    tokens = line.split(":")
                    val = tokens[1].replace("MB", "").strip()
                    #print int(float(val)/1024.0)
                    real_cloud_data = float(val)
                    ret_usage["cloud_storage_usage"]["cloud_data"] = int(float(val) / 1024.0)  # MB -> GB 
                    #print val

                if line.startswith("After de-duplication:"):
                    tokens = line.split(":")
                    #print tokens[1]
                    size = str(tokens[1]).strip().split(" ")
                    val = size[0]
                    ret_usage["cloud_storage_usage"]["cloud_data_dedup"] = int(float(val)) / 1024  # MB -> GB 
                    #print val

                if line.startswith("After compression:"):
                    tokens = line.split(":")
                    #print tokens[1]
                    size = str(tokens[1]).strip().split(" ")
                    val = size[0]
                    ret_usage["cloud_storage_usage"]["cloud_data_dedup_compress"] = int(float(val)) / 1024  # MB -> GB 
                    #print val

                if line.startswith("Cache size: current:"):
                    line = line.replace("," , ":")
                    tokens = line.split(":")
                    crt_size = tokens[2]
                    max_size = tokens[4]

                    crt_tokens = str(crt_size).strip().split(" ")
                    crt_val = crt_tokens[0]
                    ret_usage["gateway_cache_usage"]["used_cache_size"] = int(float(crt_val)) / 1024  # MB -> GB 
                    #print crt_val

                    max_tokens = str(max_size).strip().split(" ")
                    max_val = max_tokens[0]
                    ret_usage["gateway_cache_usage"]["max_cache_size"] = int(float(max_val)) / 1024  # MB -> GB 
                    #print max_val

                if line.startswith("Cache entries: current:"):
                    line = line.replace("," , ":")
                    tokens = line.split(":")
                    crt_size = tokens[2]
                    max_size = tokens[4]

                    crt_tokens = str(crt_size).strip().split(" ")
                    crt_val = crt_tokens[0]
                    ret_usage["gateway_cache_usage"]["used_cache_entries"] = crt_val
                    #print crt_val

                    max_tokens = str(max_size).strip().split(" ")
                    max_val = max_tokens[0]
                    ret_usage["gateway_cache_usage"]["max_cache_entries"] = max_val
                    #print max_val

                if line.startswith("Dirty cache status: size:"):
                    line = line.replace("," , ":")
                    tokens = line.split(":")
                    crt_size = tokens[2]
                    max_size = tokens[4]

                    crt_tokens = str(crt_size).strip().split(" ")
                    crt_val = crt_tokens[0]
                    real_cloud_data = real_cloud_data - float(crt_val)
                    ret_usage["gateway_cache_usage"]["dirty_cache_size"] = int(float(crt_val)) / 1024  # MB -> GB 
                    #print crt_val

                    max_tokens = str(max_size).strip().split(" ")
                    max_val = max_tokens[0]
                    ret_usage["gateway_cache_usage"]["dirty_cache_entries"] = max_val
                    #print max_val
                ret_usage["cloud_storage_usage"]["cloud_data"] = max(int(real_cloud_data / 1024), 0)

    except Exception:
        #print Exception
        #print "exception: %s" % CMD_CHK_STO_CACHE_STATE

        if enable_log:
            log.info(CMD_CHK_STO_CACHE_STATE + " fail")

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
    
    #log.info('get_network_speed start')
    
    # define net speed file
    netspeed_file = '/dev/shm/gw_netspeed'

    ret_val = {
               "uplink_usage" : 0 ,
               "downlink_usage" : 0,
               "uplink_backend_usage" : 0 ,
               "downlink_backend_usage" : 0
               }
    
    # test
    #return ret_val
    
    try:
        if os.path.exists(netspeed_file):
            # read net speed from file
            #log.info('Read net speed from file')
            with open(netspeed_file, 'r') as fh:
                ret_val = json.load(fh)
        else:
            # call functions directly. will consume at least one second
            #log.info('No net speed file existed. Consume one second to calculate')
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
               "msg" : "Gateway log & status",
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
        log.info("get_gateway_status")

    try:
        # get logs           
        #ret_log_dict = read_logs(NUM_LOG_LINES)
        ret_val["data"]["error_log"] = read_logs(LOGFILES, 0 , NUM_LOG_LINES)

        # get usage
        usage = storage_cache_usage()
        ret_val["data"]["cloud_storage_usage"] = usage["cloud_storage_usage"]
        ret_val["data"]["gateway_cache_usage"] = usage["gateway_cache_usage"]

    except:
        log.info("Unable to get gateway status")

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
               "msg": "gateway system logs",
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

#######################################################


if __name__ == '__main__':
    #print build_gateway("1234567")
    #print apply_user_enc_key("123456", "1234567")
    
    #_createS3qlConf("172.16.228.53:8080")
#    print get_smb_user_list()
#    print set_smb_user_list("superuser", "superuser")
#    print get_smb_user_list()
    #print _traceroute_backend('aaa')
    snapshot.rebuild_snapshot_database()
    pass
