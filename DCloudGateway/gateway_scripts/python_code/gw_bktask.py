# -*- coding: utf-8 -*-
'''
    gw_bktask.py: Tasks to be periodically run in background of gateway.

    Will be launched after gateway is up.
'''

import os
import sys
import json
import time
from signal import signal, SIGTERM, SIGINT
from threading import Thread

# delta specified API
from gateway import common
from gateway import api

# 3rd party modules
#import posix_ipc

log = common.getLogger(name="BKTASK", conf="/etc/delta/Gateway.ini")

# global flag to exit while safely
g_program_exit = False
g_prev_flushing = False
g_prev_cache_full_log = False
g_prev_entries_full_log = False

####################################################################
'''
    Functions for daemonize process.
    Copied from S3QL source code.
'''


def daemonize(workdir='/'):
    '''Daemonize the process'''

    os.chdir(workdir)

    detach_process_context()

    # wthung, disable redirection because after redirection,
    #   some eror occurred when calling sudo by subprocess.popen
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

def thread_cache_usage():
    """
    Worker thread to monitor cache size/entries usage
    """
    global g_program_exit
    global g_prev_cache_full_log
    global g_prev_entries_full_log
    
    criteria_high = 99.9
    criteria_low = 80.0
    
    while not g_program_exit:
        usage = api._get_storage_capacity()
        max_cache_size = usage['gateway_cache_usage']['max_cache_size']
        max_entries = usage['gateway_cache_usage']['max_cache_entries']
        cur_dirty_cache_size = usage['gateway_cache_usage']['dirty_cache_size']
        cur_dirty_cache_entries = usage['gateway_cache_usage']['dirty_cache_entries']
        
        if max_cache_size > 0 and max_entries > 0:
            cache_percent = (float(cur_dirty_cache_size) / float(max_cache_size)) * 100
            entries_percent = (float(cur_dirty_cache_entries) / float(max_entries)) * 100
            
            # check cache size
            if cache_percent <= criteria_low:
                g_prev_cache_full_log = False
            elif cache_percent >= criteria_high and not g_prev_cache_full_log:
                log.info('Full cache')
                g_prev_cache_full_log = True

            # check cache entries number
            if entries_percent <= criteria_low:
                g_prev_entries_full_log = False
            elif entries_percent >= criteria_high and not g_prev_entries_full_log:
                log.info('Full cache entries')
                g_prev_entries_full_log = True
        else:
            log.error('Get negative max cache size or max entries')
    
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
            print 'detected dhclient'
            # dhclient exists, terminate it
            ret_code, _ = api._run_subprocess('sudo pkill -9 dhclient')
            if not ret_code:
                print('dhclient process has been terminated')
            else:
                print('Detected a dhclient process is running, but terminating it is failed')
        
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
        os.system("sudo apt-get update 1>/dev/null 2>/dev/null")
        os.system("sudo apt-show-versions -u dcloud-gateway > %s" % gateway_version_file)
        
        # sleep for some time by a for loop in order to break at any time
        for _ in range(600):
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
            log.error('Got exception from calculate_net_speed()')

        # serialize json object to file
        with open(netspeed_file, 'w') as fh:
            json.dump(ret_val, fh)

        # wait
        time.sleep(3)

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


def get_gw_indicator():
    """
    Get gateway indicators and save it to /dev/shm/gw_indicator

    @rtype: boolean
    @return: True if successfully get indicators. Otherwise, false.
    """

    indic_file = '/dev/shm/gw_indicator'
    last_backup_time_file = '/root/.s3ql/gw_last_backup_time'
    op_ok = False
    op_msg = 'Gateway indicators read failed unexpectedly.'
    return_val = {
        'result': op_ok,
        'msg': op_msg,
        'data': {'network_ok': False,
        'system_check': False,
        'flush_inprogress': False,
        'dirtycache_nearfull': False,
        'HDD_ok': False,
        'NFS_srv': False,
        'SMB_srv': False,
        'snapshot_in_progress': False,
        'HTTP_proxy_srv': False,
        'S3QL_ok': False}}

    global g_prev_flushing

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
        

    except Exception as err:
        log.error("Unable to get indicators")
        log.error("Error message: %s" % str(err))

    return return_val['result']
    
    
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
            # grep the serial number
            for line in output:
                if target_str in line:
                    enable_ok = True
                    break                              
        else:
            #log.error('Some error occurred when enable the SMART control of %s' % disk)
            pass

    except:
        pass    
        
    return enable_ok

    
def check_RAID_rebuild(disk):
    """
    Check if disk is rebuilding RAID by mdadm utility
           
    @type disk: string
    @param disk: HDD device name. e.g., /dev/sda
    @rtype: boolean
    @return: True if disk is rebuilding RAID, false if not.
    """
    
    target_str = 'rebuilding'
    dev_no = ''
    is_rebuilding = False
    
    try:
        for i in range(2):            
            cmd = "sudo mdadm --detail /dev/md%d" % i
            ret_code, output = api._run_subprocess(cmd)
    
            if ret_code == 0:
                # grep the serial number
                for line in output.split('\n'):
                    if target_str not in line:
                        continue
                    else:
                        dev_no = line.split()[6][:8]
                        if dev_no == disk:
                            is_rebuilding = True
                            break
            else:
                log.error('Some error occurred when getting serial number of %s' % disk)
    except Exception:
        pass
            
    return is_rebuilding
     
     
def get_HDD_status():
    """
    For each disk, enable the SMART control optinos, get the serial number and check the status.
    Finally, compare with status collected before to find if some disk is missing.
    The status would be encoded to json object and written to /dev/shm/gw_HDD_status.    
    """
    
    global g_program_exit
    
    op_ok = True
    
    _data = [] # serial and status of disks
    pre_all_disks = set() 
    all_disks = set() 
           
    return_val = {
        'result': '',
        'msg': '',
        'msg_code': '',
        'data': '',
        }
    
    while not g_program_exit:
        
        try:
            all_disk = common.getAllDisks()
            _data = []

            for i in all_disk:
        
                enable_ok = enableSMART(i)
                if not enable_ok:
                    op_ok = False
                                          
                serial_num = api._get_serial_number(i)            
                all_disks.add(serial_num)
            
                cmd = "sudo smartctl -H %s" % i
                ret_code, output = api._run_subprocess(cmd)                

                if output.find("SMART overall-health self-assessment test result: PASSED") != -1:
                    if not check_RAID_rebuild(i):
                        single_hdd = {'serial': serial_num, 'status': 0} # HDD is normal
                    else:                         
                        single_hdd = {'serial': serial_num, 'status': 2} # HDD is rebuilding RAID            
                else:
                    #log.error("%s (SN: %s) SMART test result: NOT PASSED" % (i, get_serial_number(i)))  
                    single_hdd = {'serial': serial_num, 'status': 1} # HDD is failed  
                
                _data.append(single_hdd)        
        
        except Exception as e:
            op_ok = False
                
        # first read gw_HDD_status file to check if there are missing disks
        if os.path.exists('/root/gw_HDD_status'):
            with open('/root/gw_HDD_status', 'r') as fh:
                previous_status = json.loads(fh.read())
                for sn in previous_status['data']:
                    pre_all_disks.add(sn['serial'])
            
                if (len(all_disks) < len(pre_all_disks)):
                    missing_disk = (all_disks^pre_all_disks)
                    for missing in missing_disk:
                        single_hdd = {'serial': missing, 'status': 3} # HDD is not installed or empty slot
                        _data.append(single_hdd)     
            
        return_val['result'] = True
        return_val['data'] = _data    
    
        # update gw_HDD_status file
        with open('/root/gw_HDD_status', 'w') as fh:
            json.dump(return_val, fh)
            
        # sleep for some time by a for loop in order to break at any time
        for _ in range(30):
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
    atexit.register(handler_sigterm)
    # normal exit when killed
    signal(SIGTERM, signal_handler)
    signal(SIGINT, signal_handler)

    #~ # create a thread to calculate net speed
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
    #~ 
    #~ # create a thread to get HDD status
    t5 = Thread(target=get_HDD_status)
    t5.start()

    while not g_program_exit:
        # get gateway indicators
        ret = get_gw_indicator()
        if ret:
            pass
        else:
            log.error("Failed to get indicator")

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
