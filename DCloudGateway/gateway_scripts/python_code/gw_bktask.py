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
        #log.info('Daemonizing, new PID is %d', pid)
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
    Worker thread to do 'apt-get update'
    """

    global g_program_exit
    
    while not g_program_exit:
        os.system("sudo apt-get update >/dev/null")
        
        # sleep for some time by a for loop in order to break at any time
        for x in range(600):
            time.sleep(1)
            if g_program_exit:
                break


def thread_netspeed():
    """
    Worker thread to calculate network speed by calling api.py.
    Currently we always return network speed of eth1.
    """

    global g_program_exit
    #log.info('Net speed thread start')

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
            log.info('[2] Got exception from calculate_net_speed()')

        # serialize json object to file
        with open(netspeed_file, 'w') as fh:
            json.dump(ret_val, fh)

        # wait
        time.sleep(3)

    #log.info('Net speed thread end')

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
        'HTTP_proxy_srv': False}}

    try:
        return_val = api.get_indicators()

    except Exception as err:
        log.info("[2] Unable to get indicators")
        log.info("[2] Error message: %s" % str(err))

    with open(indic_file, 'w') as fh:
        json.dump(return_val, fh)

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
    #log.info('Start gateway background task')
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
        #log.info('Start to get gateway indicator')
        ret = get_gw_indicator()
        if ret:
            pass
            #log.info("Get indicator OK")
        else:
            log.info("[2] Failed to get indicator")

        # in single loop mode, set global flag to true
        if singleloop:
            g_program_exit = True
            break

        # sleep for some time by a for loop in order to break at any time
        for x in range(20):
            time.sleep(1)
            if g_program_exit:
                break

    #log.info('Gateway background task program exits successfully')

if __name__ == '__main__':
    start_background_tasks()
