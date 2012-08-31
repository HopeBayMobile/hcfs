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
    #redirect_stream(sys.stdin, None)
    #redirect_stream(sys.stdout, None)
    #redirect_stream(sys.stderr, None)


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

#def thread_get_snapshot_status():
#   # create a message queue to receive events
#   mq = posix_ipc.MessageQueue('/bktask_mq', posix_ipc.O_CREAT)
#   while not g_program_exit:
#       try:
#           # receive message by one second timeout
#           (msg, priority) = mq.receive(1)
#           if 'snapshot' in msg:
#               os.system('touch /dev/shm/123')
#       except:
#           pass
#
#   # cleanup
#   mq.close()
#   mq.unlink()
#   print 'snapshot thread exited'

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
    #atexit.register(handler_sigterm)
    # normal exit when killed
    signal(SIGTERM, signal_handler)
    signal(SIGINT, signal_handler)

    # create a thread to calculate net speed
    t = Thread(target=thread_netspeed)
    t.start()
    
    # create a thread to do 'apt-get update'
    t2 = Thread(target=thread_aptget)
    t2.start()

#    t2 = Thread(target=thread_get_snapshot_status)
#    t2.start()

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
