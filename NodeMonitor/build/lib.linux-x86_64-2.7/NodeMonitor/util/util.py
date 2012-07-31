import os
import sys
import subprocess
import logging
import logging.handlers
import threading
import random
import signal
import socket
import math
import pickle
import time
import functools
import errno
from nodeMonitorCfg import NodeMonitorCfg


WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
DELTADIR = '/etc/delta'


def enum(**enums):
    return type('Enum', (), enums)

GlobalVar = enum(
         FORMATTER='[%(levelname)s from %(name)s on %(asctime)s] %(message)s',
         DELTADIR=DELTADIR,
         CONF='%s/node_monitor.ini' % DELTADIR,
         )

FORMATTER = GlobalVar.FORMATTER
CONF = GlobalVar.CONF

# tryLock decorator
def tryLock(tries=11, lockLife=900):
    def deco_tryLock(fn):
        def wrapper(*args, **kwargs):

            returnVal = None
            locked = 1
            try:
                os.system("mkdir -p %s" % os.path.dirname(lockFile))
                cmd = "lockfile -11 -r %d -l %d %s" % (tries, lockLife, lockFile)
                locked = os.system(cmd)
                if locked == 0:
                    returnVal = fn(*args, **kwargs)  # first attempt
                else:
                    raise TryLockError()
                return returnVal
            finally:
                if locked == 0:
                    os.system("rm -f %s" % lockFile)

        return wrapper  # decorated function

    return deco_tryLock  # @retry(arg[, ...]) -> true decorator


# Retry decorator
def retry(tries, delay=3):
    '''
    Retries a function or method until it returns True.
    delay sets the delay in seconds, and backoff sets the factor by which
    the delay should lengthen after each failure. tries must be at least 0, and delay
    greater than 0.
    '''

    tries = math.floor(tries)
    if tries < 0:
        raise ValueError("tries must be 0 or greater")
    if delay <= 0:
        raise ValueError("delay must be greater than 0")

    def deco_retry(f):
        def wrapper(*args, **kwargs):
            mtries, mdelay = tries, delay  # make mutable
            rv = f(*args, **kwargs)  # first attempt
            while mtries > 0:
                if rv is 0 or rv is True:  # Done on success
                    return rv
                mtries -= 1      # consume an attempt
                time.sleep(mdelay)  # wait...
                rv = f(*args, **kwargs)  # Try again
            return rv  # Ran out of tries
        return wrapper  # decorated function

    return deco_retry  # @retry(arg[, ...]) -> true decorator


#timeout decorator
def timeout(timeout_time):
    def timeoutDeco(f):
        @functools.wraps(f)
        def wrapper(*args, **kwargs):
            class InterruptableThread(threading.Thread):
                def __init__(self, f, *args, **kwargs):
                    threading.Thread.__init__(self)
                    self.f = f
                    self.args = args
                    self.kwargs = kwargs
                    self.result = None

                def run(self):
                    result = None
                    try:
                        self.result = (0, self.f(*(self.args), **(self.kwargs)))

                    except Exception as e:
                        self.result = (1, e, sys.exc_info()[2])

            timeout = timeout_time
            if timeout <= 0:
                timeout = 86400 * 7  # one week
            it = InterruptableThread(f, *args, **kwargs)
            it.daemon = True
            it.start()
            it.join(timeout)
            if it.isAlive():
                raise TimeoutError(timeout)
            elif it.result[0] == 0:
                return it.result[1]
            else:
                raise it.result[1], None, it.result[2]

        return wrapper
    return timeoutDeco


#TODO: add error checking
def createRamdiskDirs():
    '''
    Create some necessary directories using ramdisks
    '''
    if not os.path.exists("/dev/shm/srv"):
        os.system("mkdir /dev/shm/srv")
        os.system("mount --bind /dev/shm/srv /srv")

    if not os.path.exists("/dev/shm/DCloudSwift"):
        os.system("mkdir /dev/shm/DCloudSwift")
        os.system("mount --bind /dev/shm/DCloudSwift /DCloudSwift")


#TODO: findout a beter way to check if a daemon is alive
def isDaemonAlive(daemonName):
    logger = getLogger(name="isDaemonAlive")

    cmd = 'ps -ef | grep %s' % daemonName
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    if po.returncode != 0:
        return False

    if len(lines) > 2:
        return True
    else:
        return False

def getLogger(name=None, conf=CONF):
    """
    Get a file logger.

    @type name: string
    @param name: logger name
    @type  conf: string
    @param conf: path to the swift cluster config
    @rtype:  logging.Logger
    @return: If conf is not specified or does not exist then
             return a logger which writes log to /var/log/deltaSwift with log-level=INFO.
             Otherwise, return a logger with setting specified in conf.
             The log rotates every 1MB and with 5 bacup.
    """

    try:

        logger = logging.getLogger(name)

        if not hasattr(getLogger, 'handler4Logger'):
            getLogger.handler4Logger = {}

        if logger in getLogger.handler4Logger:
            return logger

        if os.path.isfile(conf):
            kwparams = NodeMonitorCfg(conf).getKwparams()
            logDir = kwparams.get('logDir', '/var/log/deltaSwift/')
            logName = kwparams.get('logName', 'nodeMonitor.log')
            logLevel = kwparams.get('logLevel', 'INFO')
        else:
            logDir = '/var/log/deltaSwift/'
            logName = 'nodeMonitor.log'
            logLevel = 'INFO'

        os.system("mkdir -p " + logDir)
        os.system("touch " + logDir + '/' + logName)

        hdlr = logging.handlers.RotatingFileHandler(logDir + '/' + logName, maxBytes=1024 * 1024, backupCount=5)
        hdlr.setFormatter(logging.Formatter(FORMATTER))
        logger.addHandler(hdlr)
        logger.setLevel(logLevel)
        logger.propagate = False

        getLogger.handler4Logger[logger] = hdlr
        return logger
    finally:
        pass


def getReceiverUrl():
    '''
    Parse CONF to find and return receiverUrl.
    Return None if failed.
    '''
    logger = getLogger(name="getReceiverUrl")
    receiverUrl = None
    try:
        NMC = NodeMonitorCfg(CONF)
        kwparams = NMC.getKwparams()
        receiverUrl = kwparams["receiverUrl"]
    except Exception as e:
        logger.error(str(e))

    return receiverUrl


def getSensorInterval():
    '''
    Parse CONF to find and return sensor interval.
    Return None if failed.
    '''
    logger = getLogger(name="getSensorInterval")
    interval = (None, None)
    try:
        NMC = NodeMonitorCfg(CONF)
        kwparams = NMC.getKwparams()
        interval = kwparams["interval"]

    except Exception as e:
        logger.error(str(e))

    return interval

def getIpAddress():
    logger = getLogger(name="getIpAddress")
    ipaddr = socket.gethostbyname(socket.gethostname())
    if not ipaddr.startswith("127"):
        return ipaddr

    arg = 'ip route list'
    p = subprocess.Popen(arg, shell=True, stdout=subprocess.PIPE)
    data = p.communicate()
    sdata = data[0].split()
    ipaddr = sdata[sdata.index('src') + 1]
    #netdev = sdata[ sdata.index('dev')+1 ]
    return ipaddr


def mkdirs(path):
    """
    Ensures the path is a directory or makes it if not. Errors if the path
    exists but is a file or on permissions failure.

    :param path: path to create
    """
    if not os.path.isdir(path):
        try:
            os.makedirs(path)
        except OSError, err:
            if err.errno != errno.EEXIST or not os.path.isdir(path):
                raise

def hostname2Ip(hostname, nameserver="192.168.11.1"):
    '''
    Translate hostname into ip address according to nameserver.

    @type  hostname: string
    @param hostname: the hostname to be translated
    @type  nameserver: string
    @param nameserver: ip address of the nameserver, and the default
        value is 192.168.11.1
    @rtype: string
    @return: If the translation is successfully done, then the ip
        address will be returned. Otherwise, the returned value will
        be none.
    '''
    logger = getLogger(name="hostname2Ip")

    cmd = "host %s %s" % (hostname, nameserver)
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (stdoutData, stderrData) = po.communicate()

    if po.returncode != 0:
        logger.error(stderrData)
        return None
    else:
        logger.info(stdoutData)
        lines = stdoutData.split("\n")

    for line in lines:
        if hostname in line:
            ip = line.split()[3]
            if ip.find("255.255.255.255") != -1:
                return None
            else:
                return ip

    return None


class TimeoutError(Exception):
    def __init__(self, cmd=None, timeout=None):
        self.cmd = cmd
        self.timeout = timeout
        if self.timeout is not None:
            self.timeout = str(self.timeout)

    def __str__(self):
        if self.cmd is not None and self.timeout is not None:
            return "Failed to complete \"%s\" in %s secondssss" % (self.cmd, self.timeout)
        elif self.cmd is not None:
            return "Failed to complete in \"%s\" seconds" % self.cmd
        elif self.timeout is not None:
            return "Failed to finish in %s seconds" % (self.timeout)
        else:
            return "TimeoutError"

class TryLockError(Exception):
    def __str__(self):
        return "Failed to tryLock lockFile"


if __name__ == '__main__':
    print getSensorInterval()
    #print hostname2Ip("GDFGSDFSD")
    #print restartSwiftProxy()
    pass
