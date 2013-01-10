# -*- coding: utf-8 -*-
'''
    gw_bktask.py: Tasks to be periodically run in background of gateway.

    Will be launched after gateway is up.
'''

import os
import sys
import json
import time
import ConfigParser
from signal import signal, SIGTERM, SIGINT
from threading import Thread

# delta specified API
from gateway import common
from gateway import api
from gateway import api_remote_upgrade
from gateway import update_s3ql_bandwidth as bw

log = common.getLogger(name="BKTASK", conf="/etc/delta/Gateway.ini")

# global flag to exit while safely
g_program_exit = False
# global vars
g_prev_flushing = False
g_prev_cache_full_log = False
g_prev_entries_full_log = False
g_swift_disconnect_count = 0
g_swift_connect_count = 0
g_prev_savebox_on = True

####################################################################
'''
    Functions for daemonize process.
    Copied from S3QL source code.
'''


def daemonize(workdir='/'):
    '''Daemonize the process'''

    os.chdir(workdir)
    detach_process_context()

    redirect_stream(sys.stdin, None)
    redirect_stream(sys.stdout, None)
    redirect_stream(sys.stderr, None)


def detach_process_context():
    """ Detach the process context from parent and session.

        Detach from the parent process and session group, allowing the
        parent to exit while this process continues running.

        Reference: “Advanced Programming in the Unix Environment”,
        section 13.3, by W. Richard Stevens, published 1993 by
        Addison-Wesley.
        """

    # Protected member
    #pylint: disable=W0212

    pid = os.fork()
    if pid > 0:
        os._exit(0)

    os.setsid()

    pid = os.fork()
    if pid > 0:
        os._exit(0)


def redirect_stream(system_stream, target_stream):
    """ Redirect a system stream to a specified file.

        `system_stream` is a standard system stream such as
        ``sys.stdout``. `target_stream` is an open file object that
        should replace the corresponding system stream object.

        If `target_stream` is ``None``, defaults to opening the
        operating system's null device and using its file descriptor.

        """
    if target_stream is None:
        target_fd = os.open(os.devnull, os.O_RDWR)
    else:
        target_fd = target_stream.fileno()
    os.dup2(target_fd, system_stream.fileno())

##############################################################################
'''
    Worker threads.
'''

def thread_upload_gw_usage(data_size):
    """
    Worker thread to upload gateway usage to swift
    """
    _upload_usage_data(data_size)

def thread_cache_usage():
    """
    Worker thread to monitor cache size/entries usage
    """
    global g_program_exit
    global g_prev_cache_full_log
    global g_prev_entries_full_log
    
    criteria_high = 99.9
    criteria_low = 80.0
    prev_data_size = -1
    report_time = 0
    
    while not g_program_exit:
        usage = api._get_storage_capacity()
        max_cache_size = usage['gateway_cache_usage']['max_cache_size']
        max_entries = usage['gateway_cache_usage']['max_cache_entries']
        cur_dirty_cache_size = usage['gateway_cache_usage']['dirty_cache_size']
        cur_dirty_cache_entries = usage['gateway_cache_usage']['dirty_cache_entries']
        data_size = usage['cloud_storage_usage']['cloud_data']
        
        # ensure value is not negative
        data_size = max(data_size, 0)
        
        if prev_data_size != data_size:
            report_time += 1
            # report usage by 60s
            if report_time % 3 == 0:
                # start a thread to upload data size to swift
                the_thread = Thread(target=thread_upload_gw_usage, args=(data_size,))
                the_thread.start()
                report_time = 0
            prev_data_size = data_size
        
        if max_cache_size > 0 and max_entries > 0:
            cache_percent = (float(cur_dirty_cache_size) / float(max_cache_size)) * 100
            entries_percent = (float(cur_dirty_cache_entries) / float(max_entries)) * 100
            
            # check cache size
            if cache_percent <= criteria_low:
                g_prev_cache_full_log = False
            elif cache_percent >= criteria_high and not g_prev_cache_full_log:
                log.debug('Full cache')
                g_prev_cache_full_log = True

            # check cache entries number
            if entries_percent <= criteria_low:
                g_prev_entries_full_log = False
            elif entries_percent >= criteria_high and not g_prev_entries_full_log:
                log.debug('Full cache entries')
                g_prev_entries_full_log = True
        else:
            log.debug('Get negative max cache size or max entries')
    
        # sleep for some time by a for loop in order to break at any time
        for _ in range(20):
            time.sleep(1)
            if g_program_exit:
                break

def thread_term_dhclient():
    """
    Worker thread to terminate dhclient process to avoid it to change our static IP
    """
    global g_program_exit
    
    while not g_program_exit:
        ret_code, _ = api._run_subprocess('sudo ps aux | grep dhclient')
        if not ret_code:
            # dhclient exists, terminate it
            ret_code, _ = api._run_subprocess('sudo pkill -9 dhclient')
        
        # sleep for some time by a for loop in order to break at any time
        for _ in range(30):
            time.sleep(1)
            if g_program_exit:
                break

def thread_aptget():
    """
    Worker thread to do 'apt-get update' and 'apt-show-versions'
    """

    global g_program_exit

    gateway_version_file = '/dev/shm/gateway_ver'
    
    while not g_program_exit:
        ## test if APT server alive
        apt_file = '/etc/apt/sources.list.d/delta-server-precise.list'
        fh = open(apt_file, 'r')
        line = fh.readline()
        apt_url = line.split(' ')[1]
        os.system('rm -r /tmp/test_apt')
        os.system('mkdir /tmp/test_apt')
        cmd = "wget " + apt_url
        rsp = os.system(cmd)
        ## read upgrade info
        if rsp==0:  ## code=0 means exit SUCCESS
            os.system("sudo apt-get update 1>/dev/null 2>/dev/null")
            os.system("sudo apt-show-versions -u dcloud-gateway > %s" % gateway_version_file)
        else:
            os.system("echo '' > %s" % gateway_version_file)
            
        # sleep for some time by a for loop in order to break at any time
        for _ in range(120):
            time.sleep(1)
            if g_program_exit:
                break


def thread_netspeed():
    """
    Worker thread to calculate network speed by calling api.py.
    Currently we always return network speed of eth1.
    """

    global g_program_exit

    # define net speed file
    netspeed_file = '/dev/shm/gw_netspeed'

    ret_val = {"uplink_usage": 0,
               "downlink_usage": 0,
               "uplink_backend_usage": 0,
               "downlink_backend_usage": 0}

    while not g_program_exit:
        try:
            # todo: may need to return speed of eth0 as well
            ret_val = api.calculate_net_speed('eth1')
        except:
            log.debug('Got exception from calculate_net_speed()')

        # serialize json object to file
        with open(netspeed_file, 'w') as fh:
            json.dump(ret_val, fh)

        # wait
        time.sleep(3)

def thread_retrieve_quota():
    """
    Worker thread to retrieve gateway quota from swift
    """
    global g_program_exit
    prev_quota = -1
    
    while not g_program_exit:
        quota = _get_gateway_quota()
        if quota > 0:
            # update quota to s3ql if different quota arrival
            if prev_quota != quota:
                # set quota to s3ql
                api._run_subprocess('sudo s3qlctrl quotasize /mnt/cloudgwfiles %d' % (quota / 1024), 10)
                prev_quota = quota
        
        # sleep for some time by a for loop in order to break at any time
        for _ in range(60):
            time.sleep(1)
            if g_program_exit:
                break

def thread_swift_s3ql_monitor():
    """
    Worker thread to monitor swift and s3ql status.
    """
    global g_program_exit
    global g_swift_disconnect_count
    global g_swift_connect_count
    global g_prev_savebox_on
    
    while not g_program_exit:
        # check if s3ql's cache or entry is >98% full
        if os.path.exists('/dev/shm/s3ql_cache_almost_full'):
            # swift disconnected by 3 continuous tries
            if g_swift_disconnect_count >= 3:
                g_swift_disconnect_count = 0
                # if previous savebox is on
                if g_prev_savebox_on:
                    g_prev_savebox_on = False
                    set_savebox_status(False)
            elif g_swift_connect_count >= 3:
                g_swift_connect_count = 0
                # if previous savebox is not on
                if not g_prev_savebox_on:
                    g_prev_savebox_on = True
                    set_savebox_status(True)
        # if s3ql cache or entry is <98% full, check if savebox is off
        # if so, start it
        else:
            if not g_prev_savebox_on:
                g_prev_savebox_on = True
                set_savebox_status(True)
        
        # sleep for some time by a for loop in order to break at any time
        for _ in range(20):
            time.sleep(1)
            if g_program_exit:
                break

##############################################################################
'''
    Functions.
'''


def signal_handler(signum, frame):
    """
    SIGTERM signal handler.
    Will set global flag to True to exit program safely
    """

    global g_program_exit
    g_program_exit = True

def set_savebox_status(turnon):
    """
    Set savebox status.
    
    @type turnon: Boolean
    @param turnon: Savebox status to be set
    """
    if turnon:
        # notify savebox with status 0
        api._notify_savebox(0, "Cloud storage connected.")
        # show system normal led (code = 2)
        api._show_led(2)
    else:
        # notify savebox with status 3
        api._notify_savebox(3, "Cloud storage disconnected and SAVEBOX dirty caches/entries are over 98% full.")
        # show system error led (code = 3)
        api._show_led(3)

def get_gw_indicator():
    """
    Get gateway indicators and save it to /dev/shm/gw_indicator

    @rtype: boolean
    @return: True if successfully get indicators. Otherwise, false.
    """

    indic_file = '/dev/shm/gw_indicator'
    last_backup_time_file = '/root/.s3ql/gw_last_backup_time'
    op_ok = False
    op_msg = 'Reading SAVEBOX indicators failed.'
    return_val = {}

    global g_prev_flushing
    global g_swift_disconnect_count
    global g_swift_connect_count

    try:
        return_val = api.get_indicators()
        
        with open(indic_file, 'w') as fh:
            json.dump(return_val, fh)

        # if previous flushing status is True
        if g_prev_flushing:
            if not return_val['data']['flush_inprogress']:
                # flushing from True -> False
                # write current time to file
                with open(last_backup_time_file, 'w') as fh2:
                    fh2.write(str(time.time()))
                
        # change flushing status
        g_prev_flushing = return_val['data']['flush_inprogress']
        
        # check network_ok
        if return_val['data']['network_ok']:
            g_swift_connect_count += 1
            g_swift_disconnect_count = 0
        else:
            g_swift_connect_count = 0
            g_swift_disconnect_count += 1

    except Exception as err:
        log.debug("Unable to get indicators")
        log.debug("Error message: %s" % str(err))

    if 'result' in return_val:
        return return_val['result']
    return False

def _get_storage_info():
    """
    Get storage URL and user name from /root/.s3ql/authinfo2.

    @rtype: tuple
    @return: Storage URL and user name or None if failed.
    """
    storage_url = None
    account = None
    password = None

    try:
        config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            config.readfp(op_fh)

        section = "CloudStorageGateway"
        storage_url = config.get(section, 'storage-url').replace("swift://", "")
        account = config.get(section, 'backend-login')
        password = config.get(section, 'backend-password')

    except Exception as e:
        log.debug("Failed to get storage info: %s" % str(e))
    finally:
        pass
    return (storage_url, account, password)
    
def _upload_usage_data(usage):
    """
    Upload gateway usage to swift
    
    @type usage: integer
    @param usage: Gateway usage
    """
    # temp file to store usage. 
    # ***file name of this file must be fixed because backend needs to parse it***
    target_file = 'gw_total_usage'
    # echo usage to a temp file
    os.system('sudo echo %d > %s' % (usage, target_file))
    
    # get storage info
    storage_url, account, password = _get_storage_info()
    
    # process if all required data are available
    if storage_url and account and password:
        _, username = account.split(':')
        _, output = api._run_subprocess('sudo swift -A https://%s/auth/v1.0 -U %s -K %s upload %s_private_container %s' 
                                           % (storage_url, account, password, username, target_file), 15)
        if target_file in output:
            log.debug('Uploaded SAVEBOX total usage to cloud storage (%d)' % usage)
        else:
            err_msg = 'Uploading SAVEBOX data usage to cloud storage failed.'
            log.debug(err_msg)
            log.debug('%s. %s' % (err_msg, output))
    
    # remove the temp file
    os.system('sudo rm %s' % target_file)
    
def _get_gateway_quota():
    """
    Get gateway quota by swift command
    
    @rtype: integer
    @return: -1 if fail. A integer larger than 0 if success. (Unit: byte)
    """
    # get storage info
    storage_url, account, password = _get_storage_info()
    if storage_url and account and password:
        _, username = account.split(':')
        ret_code, output = api._run_subprocess('sudo swift -A https://%s/auth/v1.0 -U %s -K %s stat %s_private_container' 
                                           % (storage_url, account, password, username), 20)
        
        if not ret_code:
            lines = output.split('\n')
            for line in lines:
                # find a fixed string
                if 'Meta Quota' in line:
                    _, meta_quota = line.split(':')
                    meta_quota = int(meta_quota)
                    return meta_quota 
        else:
            err_msg = 'Getting SAVEBOX quota failed.'
            log.debug(err_msg)
            log.debug('%s. %s' % (err_msg, output))
    
    # return -1 if quota cannot be retrieved
    return -1
    
def enableSMART(disk):
    """
    enable the SMART control opinions
    @type disk: string
    @param disk: HDD device name. e.g., /dev/sda
    @rtype: boolean
    @return: True if enable successed, false if not
    """
    
    target_str = 'SMART Enabled.'
    enable_ok = False
    
    try:
        cmd = "sudo smartctl --smart=on %s" % disk
        ret_code, output = api._run_subprocess(cmd)

        if ret_code == 0:
            # check if enable successed
            for line in output.split('\n'):
                if target_str in line:
                    enable_ok = True
                    break                              
        else:
            log.debug('Some error occurred when enable the SMART control of %s' % disk)
            
    except:
        pass    
        
    return enable_ok

    
def check_RAID_rebuild(disk):
    """
    Check if disk is rebuilding RAID by mdadm utility
           
    @type disk: string
    @param disk: HDD device name. e.g., /dev/sda
    @rtype: boolean
    @return: op_ok: return whether the mdadm utility run successed 
             is-rebuilding: True if disk is rebuilding RAID, false if not.
    """
    
    target_str = 'rebuilding'
    dev_no = ''
    is_rebuilding = False
    op_ok = False
    
    try:
        for i in range(2):            
            cmd = "sudo mdadm --detail /dev/md%d" % i
            ret_code, output = api._run_subprocess(cmd)
    
            if ret_code == 0:
                # ckeck RAID status
                op_ok = True
                for line in output.split('\n'):
                    if target_str not in line:
                        continue
                    else:
                        dev_no = line.split()[6][:8]
                        if dev_no == disk:
                            is_rebuilding = True
                            break
            else:
                log.debug('Some error occurred when checking whether %s is rebuilding RAID' % disk)
    except Exception:
        pass
            
    return (op_ok, is_rebuilding)
     
     
def get_HDD_status():
    """
    For each disk, enable the SMART control optinos, get the serial number and check the status.
    Finally, compare with status collected before to find if some disk is missing.
    The status would be encoded to json object and written to /root/gw_HDD_status.    
    """
    
    global g_program_exit
    
    op_ok = True
    _data = [] # serial and status of disks
               
    return_val = {
        'result': '',
        'msg': '',
        'msg_code': '',
        'data': '',
        }
    
    while not g_program_exit:
        
        try:
            op_ok = True
            all_disk = common.getAllDisks()
            _data = []
            all_disks = set() # save the dev name for all scaned disks. e.g. {"dev/sda", "/dev/sdb"}
                        
            for i in all_disk:
        
                op_ok = enableSMART(i)
                
                serial_num = api._get_serial_number(i)            
                all_disks.add(serial_num)
            
                cmd = "sudo smartctl -H %s" % i
                ret_code, output = api._run_subprocess(cmd)                

                if output.find("SMART overall-health self-assessment test result: PASSED") != -1:
                    op_ok, is_rebuilding = check_RAID_rebuild(i)
                    
                    if not is_rebuilding:
                        single_hdd = {'serial': serial_num, 'status': 0} # HDD is normal
                    else:                         
                        single_hdd = {'serial': serial_num, 'status': 2} # HDD is rebuilding RAID            
                else:
                    log.warning("%s (SN: %s) SMART test result: NOT PASSED" % (i, get_serial_number(i)))  
                    single_hdd = {'serial': serial_num, 'status': 1} # HDD is failed  
                
                _data.append(single_hdd)        
        
            # first read gw_HDD_status file to check if there are missing disks
            if os.path.exists('/root/gw_HDD_status'):
                with open('/root/gw_HDD_status', 'r') as fh:
                    previous_status = json.loads(fh.read())
                    if (len(all_disks) < len(previous_status['data'])):
                        for disk in previous_status['data']:
                            if disk['serial'] not in all_disks:
                                single_hdd = {'serial': disk['serial'], 'status': 3} # HDD is not installed or empty slot
                                _data.append(single_hdd)
                    
        except Exception as e:
            op_ok = False
        
        try:
            return_val['result'] = op_ok
            return_val['data'] = _data    
            
            # update gw_HDD_status file
            with open('/root/gw_HDD_status', 'w') as fh:
                json.dump(return_val, fh)
       
        except:
            pass
                    
        # sleep for some time by a for loop in order to break at any time
        for _ in range(30):
            time.sleep(1)
            if g_program_exit:
                break
             
    
def update_bandwidth():
    """
    Check the forced_upload file in /dev/shm to see if need to update the s3ql bandwidth. 
    If forced_file exists, set the full bandwidth to upload.
    If not, set the confiured bandwidth back.       
    """

    global g_program_exit

    now_bw = 1 # 0 full bandwidth / 1 configured bandwidth
    full_bw = 1024 * 1024

    while not g_program_exit:
        if os.path.exists('/dev/shm/forced_upload'):
            if now_bw == 0:
                pass
            else:
                bw.set_bandwidth(full_bw)
                cmd = "/etc/delta/uploadon"
                os.system(cmd)
                now_bw = 0

        else:
            if now_bw == 0:
                cmd = "/etc/delta/update_bandwidth"
                os.system(cmd)
                now_bw = 1
            else:
                pass

        for _ in range(10):
            time.sleep(1)
            if g_program_exit:
                break

           
##############################################################################
'''
    Main.
'''


def start_background_tasks(singleloop=False):
    """
    Main entry point.
    """

    global g_program_exit
    # todo: check arguments

    # daemonize myself
    daemonize()

    # register handler
    # normal exit when killed
    signal(SIGTERM, signal_handler)
    signal(SIGINT, signal_handler)

    # create a thread to calculate net speed
    t = Thread(target=thread_netspeed)
    t.start()
    
    # create a thread to do 'apt-get update'
    t2 = Thread(target=thread_aptget)
    t2.start()

    # create a thread to do terminate dhclient
    t3 = Thread(target=thread_term_dhclient)
    t3.start()
    
    # create a thread to do monitor dirty cache/entries usage
    t4 = Thread(target=thread_cache_usage)
    t4.start()
    
    # create a thread to get HDD status
    t5 = Thread(target=get_HDD_status)
    t5.start()
    
    # create a thread to retrieve gateway quota from swift
    t6 = Thread(target=thread_retrieve_quota)
    t6.start()
    
    # create a thread to update s3ql upload bandwidth
    t7 = Thread(target=update_bandwidth)
    t7.start()
    
    # create a thread to monitor swift and s3ql status
    t_s2_monitor = Thread(target=thread_swift_s3ql_monitor)
    t_s2_monitor.start()
         
    while not g_program_exit:
        # get gateway indicators
        ret = get_gw_indicator()
        if ret:
            pass
        else:
            log.debug("Failed to get indicator")

        # in single loop mode, set global flag to true
        if singleloop:
            g_program_exit = True
            break

        # sleep for some time by a for loop in order to break at any time
        for _ in range(20):
            time.sleep(1)
            if g_program_exit:
                break

if __name__ == '__main__':
    start_background_tasks()
