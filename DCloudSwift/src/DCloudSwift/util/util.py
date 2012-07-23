import os
import sys
import subprocess
import fcntl
import logging
import logging.handlers
import threading
import random
import signal
import socket
import struct
import math
import pickle
import time
import functools
import errno
from ConfigParser import ConfigParser
from SwiftCfg import SwiftCfg


WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
DELTADIR = '/etc/delta'


def enum(**enums):
    return type('Enum', (), enums)

GlobalVar = enum(SWIFTCONF='%s/DCloudSwift/Swift.ini' % BASEDIR,
         FORMATTER='[%(levelname)s from %(name)s on %(asctime)s] %(message)s',
         DELTADIR=DELTADIR,
         SWIFTDIR='/etc/swift',
         ORI_SWIFTCONF='%s/Swift.ini' % DELTADIR,
         MASTERCONF='%s/swift_master.ini' % DELTADIR,
         ACCOUNT_DB='%s/swift_account.db' % DELTADIR,
         NODE_DB='%s/swift_node.db' % DELTADIR,
         OBJBUILDER='object.builder')


SWIFTCONF = GlobalVar.SWIFTCONF
FORMATTER = GlobalVar.FORMATTER

lockFile = "/etc/delta/swift.lock"


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


def isValid(vers, fingerprint):
    retval = True

    if vers != fingerprint["vers"]:
        retval = False

    if socket.gethostname() != fingerprint["hostname"]:
        retval = False

    return retval


def restartAllServices():
    logger = getLogger(name="restartAllServices")
    stopAllServices()
    generateSwiftConfig()
    os.system("chown -R swift:swift /etc/swift")

    if restartRsync() != 0:
        logger.error("Failed to restart rsyncd")

    if restartMemcached() != 0:
        logger.error("Failed to restart memcached")

    os.system("chown -R swift:swift /srv/node/ ")
    restartSwiftServices()


def startAllServices():
    logger = getLogger(name="startAllServices")
    startRsync()
    startMemcached()
    startSwiftServices()


def restartSwiftProxy():
    logger = getLogger(name="restartProxy")

    cmd = "swift-init proxy start"
    po = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
    output = po.stdout.read()
    po.wait()

    if po.returncode !=0:
        return po.returncode

    cmd = "swift-init proxy stop"
    po = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
    output = po.stdout.read()
    po.wait()

    if po.returncode != 0:
        return po.returncode

    cmd = "swift-init proxy start"
    po = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
    output = po.stdout.read()
    po.wait()

    return po.returncode


def restartRsync():
    logger = getLogger(name="restartRsync")

    os.system("rm /var/run/rsyncd.pid")

    cmd = "/etc/init.d/rsync restart"
    po = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)

    output = po.stdout.read()
    po.wait()

    return po.returncode


def startRsync():
    cmd = "/etc/init.d/rsync start"
    po = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)

    output = po.stdout.read()
    po.wait()

    return po.returncode


def restartMemcached():
    os.system("/etc/init.d/memcached stop")
    os.system("rm /var/run/memcached.pid")

    cmd = "/etc/init.d/memcached start"
    po = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)

    output = po.stdout.read()
    po.wait()

    return po.returncode


def startMemcached():
    cmd = "/etc/init.d/memcached start"
    po = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)

    output = po.stdout.read()
    po.wait()

    return po.returncode


def restartSwiftServices():
    '''
    start appropriate swift services
    '''
    os.system("swift-init all restart")


def startSwiftServices():
    '''
    start appropriate swift services
    '''
    os.system("swift-init all start")


def runPopenCommunicate(cmd, inputString, logger):
    po = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    (stdout, stderr) = po.communicate(inputString)

    if po.returncode == 0:
        logger.debug("Succeed to run \'%s\'" % cmd)
    else:
        logger.error(stderr)

    return po.returncode


def findLine(filename, line):
    f = open(filename, 'r')
    for l in f.readlines():
        l1 = l.strip()
        l2 = line.strip()
        if l1 == l2:
            return True

    return False


def getLogger(name=None, conf=SWIFTCONF):
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
            kwparams = SwiftCfg(conf).getKwparams()
            logDir = kwparams.get('logDir', '/var/log/deltaSwift/')
            logName = kwparams.get('logName', 'deltaSwift.log')
            logLevel = kwparams.get('logLevel', 'INFO')
        else:
            logDir = '/var/log/deltaSwift/'
            logName = 'deltaSwift.log'
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


def getPortalUrl():
    '''
    Parse SWIFTCONF to find and return protalUrl.
    Return None if failed.
    '''
    logger = getLogger(name="getPortalUrl")
    portalUrl = None
    try:
        SC = SwiftCfg(SWIFTCONF)
        kwparams = SC.getKwparams()
        portalUrl = kwparams["portalUrl"]
    except Exception as e:
        logger.error(str(e))

    return portalUrl


def getProxyPort():
    '''
    get proxyPort by reading SWIFTCONF

    @rtype: integer
    @return: Parse the config file to find and return the proxy port. 
             Return None if failed.
    '''
    logger = getLogger(name="getProxyPort")
    proxyPort = None
    try:
        SC = SwiftCfg(SWIFTCONF)
        kwparams = SC.getKwparams()
        proxyPort = kwparams["proxyPort"]
    except Exception as e:
        logger.error(str(e))

    return proxyPort


def generateMasterProxyConfig():
    """
    Generate proxy config files for master.

    @rtype: integer
    @return: 0 iff the proxy Config is successfully generated.
    """
    logger = getLogger(name="generateMasterProxyConfig")
    ip = getIpAddress()
    proxyPort = getProxyPort()
    portalUrl = getPortalUrl()

    if proxyPort and ip and portalUrl:
        cmd = "sh %s/DCloudSwift/proxy/CreateMasterProxyConfig.sh %d %s %s" % (BASEDIR, proxyPort, ip, portalUrl)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        if po.returncode != 0:
            logger.error("Failed to run %s for %s" % (cmd, lines))
            return 1
    else:
        logger.error("Failed to generate master proxy config due to incompte arguments")
        return 1

    return 0


def generateSwiftConfig():
    """
    Generate config files for proxy, rsyncd, accountserver, containcerserver
    and objectserver.

    @rtype: None
    @return: no return
    """

    ip = socket.gethostbyname(socket.gethostname())
    if ip.startswith("127"):
        ip = getIpAddress()

    proxyPort = getProxyPort()
    portalUrl = getPortalUrl()

    if proxyPort and ip and portalUrl:
        os.system("sh %s/DCloudSwift/proxy/CreateProxyConfig.sh %d %s %s" % (BASEDIR, proxyPort, ip, portalUrl))
    os.system("sh %s/DCloudSwift/storage/rsync.sh %s" % (BASEDIR, ip))
    os.system("sh %s/DCloudSwift/storage/accountserver.sh %s" % (BASEDIR, ip))
    os.system("sh %s/DCloudSwift/storage/containerserver.sh %s" % (BASEDIR, ip))
    os.system("sh %s/DCloudSwift/storage/objectserver.sh %s" % (BASEDIR, ip))

    os.system("chown -R swift:swift /etc/swift")


def getDeviceCnt():
    logger = getLogger(name="getDeviceCnt")

    config = ConfigParser()
    config.readfp(open(SWIFTCONF))

    try:
        with open(SWIFTCONF, "rb") as fh:
            config.readfp(fh)
    except IOError:
        logger.error("Failed to load swift.ini")
        return None

    return int(config.get('storage', 'deviceCnt'))


def getDevicePrx():
    logger = getLogger(name="getDevicePrx")
    return "sdb"


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


def getNumOfReplica(swiftDir="/etc/swift"):
    logger = getLogger(name="getNumOfReplica")
    builderFile = swiftDir + "/" + GlobalVar.OBJBUILDER
    if not os.path.exists(builderFile):
        logger.error("Cannont find %s" % builderFile)
        return None

    cmd = 'swift-ring-builder %s' % builderFile
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    if po.returncode != 0:
        logger.error("Failed to run %s" % cmd)
        return None

    try:
        token = lines[1].split(',')[1]
        numOfReplica = int(token.split()[0])
    except Exception as e:
        logger.error("Failed to parse the output of %s" % cmd)
        return None

    return numOfReplica


def getNumOfZones(swiftDir="/etc/swift"):
    logger = getLogger(name="getNumOfZones")
    builderFile = swiftDir + "/" + GlobalVar.OBJBUILDER
    if not os.path.exists(builderFile):
        logger.error("Cannont find %s" % builderFile)
        return None

    cmd = 'swift-ring-builder %s' % builderFile
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    if po.returncode != 0:
        logger.error("Failed to run %s" % cmd)
        return None

    try:
        token = lines[1].split(',')[2]
        numOfZones = int(token.split()[0])
    except Exception as e:
        logger.error("Failed to parse the output of %s" % cmd)
        return None

    return numOfZones


def getProxyNodeIpList(swiftDir="/etc/swift"):
    """
    get the ip list of proxy nodes by reading swiftDir/proxyList

    @type swiftDir: string
    @param swiftDir: path to the directory containing the file proxyList
    @rtype: list
    @return: If swiftDir/proxyList exists and contains legal contents
             then return the ip list of proxy nodes.
             Otherwise, return none,
    """
    logger = getLogger(name="getProxyNodeIpList")

    proxyList = []
    ipSet = set()

    try:
        with open("%s/proxyList" % swiftDir, "rb") as fh:
            proxyList = pickle.load(fh)
    except IOError:
        logger.warn("Failed to load proxyList from %s/proxyList" % swiftDir)
        return None

    for node in proxyList:
        ipSet.add(node["ip"])

    ipList = [ip for ip in ipSet]

    return ipList


def getSwiftNodeIpList(swiftDir="/etc/swift"):
    logger = getLogger(name="getSwiftNodeIpList")

    proxyList = []
    ipSet = set()

    try:
        with open("%s/proxyList" % swiftDir, "rb") as fh:
            proxyList = pickle.load(fh)
    except IOError:
        logger.warn("Failed to load proxyList from %s/proxyList" % swiftDir)
        return None

    storageIpList = getStorageNodeIpList(swiftDir=swiftDir)
    if storageIpList is None:
        return None

    for ip in storageIpList:
        ipSet.add(ip)

    for node in proxyList:
        ipSet.add(node["ip"])

    ipList = [ip for ip in ipSet]

    return ipList


def getSwiftConfVers(confDir="/etc/swift"):
    logger = getLogger(name="getSwiftConfVers")

    cmd = 'cd %s; swift-ring-builder object.builder' % confDir
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    if po.returncode != 0:
        logger.error("Failed to get version info from %s/objcet.builder" % confDir)
        return -1

    tokens = lines[0].split()
    vers = int(tokens[3])

    versBase = 0
    try:
        with open("%s/versBase" % confDir, "rb") as fh:
            versBase = pickle.load(fh)
    except IOError:
        logger.error("Failed to load version base from %s/versBase" % confDir)
        return -1
    except OSError:
        logger.error("Failed to load version base from %s/versBase" % confDir)
        return -1

    return vers + versBase


def getIp2Zid(swiftDir="/etc/swift"):
    '''
    Collect ip 2 zone_id mapping
    '''

    logger = getLogger(name='getIp2Zid')

    builderFile = swiftDir + "/" + GlobalVar.OBJBUILDER
    if not os.path.exists(builderFile):
        logger.error("Cannont find %s" % builderFile)
        return None

    cmd = 'swift-ring-builder %s' % builderFile
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    if po.returncode != 0:
        logger.error("Failed to run %s" % cmd)
        return None

    ip2Zid = {}
    try:
        if len(lines) < 5:
            return ip2Zid
        for line in lines[4:]:
            tokens = line.split()
            zid = int(tokens[1])
            ip = tokens[2]
            ip2Zid[ip] = zid
    except Exception as e:
        logger.error("Failed to parse the output of %s" % cmd)
        return None

    return ip2Zid


def getStorageNodeIpList(swiftDir="/etc/swift"):
    '''
    Collect ip list of all storge nodes
    '''

    logger = getLogger(name='getStorageNodeIpList')

    cmd = 'cd %s; swift-ring-builder object.builder' % swiftDir
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    if po.returncode != 0:
        logger.error("Failed to obtain storage node list from %s/object.builder" % swiftDir)
        return None

    i = 0
    ipList = []
    for line in lines:
        if i > 3:
            ipList.append(line.split()[2])

        i += 1

    return ipList


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


def sshpass(passwd, cmd, timeout=0):
    logger = getLogger(name="sshpass")

    class InterruptableThread(threading.Thread):
        def __init__(self, passwd, cmd):
            threading.Thread.__init__(self)
            self.cmd = cmd
            self.passwd = passwd
            self.result = None

        def run(self):
            try:
                sshpasscmd = "sshpass -p %s %s" % (self.passwd, self.cmd)
                po = subprocess.Popen(sshpasscmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                (stdoutData, stderrData) = po.communicate()

                self.result = (po.returncode, stdoutData, stderrData)
            except Exception as e:
                self.result = (1, 0, str(e))

    if timeout <= 0:
        timeout = 86400 * 7  # one week
    it = InterruptableThread(passwd, cmd)
    it.daemon = True
    it.start()
    it.join(timeout)
    if it.isAlive():
        raise TimeoutError(cmd=cmd, timeout=timeout)
    else:
        return it.result


def spreadMetadata(password, sourceDir="/etc/swift/", nodeList=[]):
    logger = getLogger(name="spreadMetadata")
    blackList = []
    returncode = 0
    for ip in nodeList:
        try:
            cmd = "ssh root@%s mkdir -p /etc/swift/" % ip
            (status, stdout, stderr) = sshpass(password, cmd, timeout=20)
            if status != 0:
                raise SshpassError(stderr)

            logger.info("scp -o StrictHostKeyChecking=no --preserve %s/* root@%s:/etc/swift/" % (sourceDir, ip))
            cmd = "scp -o StrictHostKeyChecking=no --preserve %s/* root@%s:/etc/swift/" % (sourceDir, ip)
            (status, stdout, stderr) = sshpass(password, cmd, timeout=360)
            if status != 0:
                raise SshpassError(stderr)

            cmd = "ssh root@%s chown -R swift:swift /etc/swift " % (ip)

            (status, stdout, stderr) = sshpass(password, cmd, timeout=20)
            if status != 0:
                raise SshpassError(stderr)

        except TimeoutError as err:
            blackList.append(ip)
            returncode += 1
            logger.error("Failed to execute \"%s\" in time" % (cmd))
            continue
        except SshpassError as err:
            blackList.append(ip)
            returncode += 1
            logger.error("Failed to execute \"%s\" for %s" % (cmd, err))
            continue

    return (returncode, blackList)


def spreadPackages(password, nodeList=[]):
    logger = getLogger(name="spreadPackages")
    blackList = []
    returncode = 0
    cmd = ""
    for ip in nodeList:
        try:
            print "Start installation of swfit packages on %s ..." % ip

            cmd = "ssh root@%s mkdir -p /etc/lib/swift/" % (ip)
            (status, stdout, stderr) = sshpass(password, cmd, timeout=60)
            if status != 0:
                raise SshpassError(stderr)

            logger.info("scp -o StrictHostKeyChecking=no -r /etc/lib/swift/* root@%s:/etc/lib/swift/" % (ip))
            cmd = "scp -o StrictHostKeyChecking=no -r /etc/lib/swift/* root@%s:/etc/lib/swift/" % (ip)
            (status, stdout, stderr) = sshpass(password, cmd, timeout=60)
            if status != 0:
                raise SshpassError(stderr)

            cmd = "ssh root@%s dpkg -i /etc/lib/swift/*.deb " % (ip)

            (status, stdout, stderr) = sshpass(password, cmd, timeout=360)
            if status != 0:
                raise SshpassError(stderr)

        except TimeoutError as err:
            blackList.append(ip)
            returncode += 1
            logger.error("Failed to execute \"%s\" in time" % (cmd))
            print "Failed to install swift packages on %s" % ip
            continue
        except SshpassError as err:
            blackList.append(ip)
            returncode += 1
            logger.error("Failed to execute \"%s\" for %s" % (cmd, err))
            print "Failed to install swift packages on %s" % ip
            continue

    return (returncode, blackList)


def spreadRC(password, nodeList=[]):
    logger = getLogger(name="spreadRC")
    blackList = []
    returncode = 0
    cmd = ""
    for ip in nodeList:
        try:
            print "Start spreading rc.local to %s ..." % ip
            logger.info("scp -o StrictHostKeyChecking=no /etc/lib/swift/BootScripts/rc root@%s:/etc/init.d/rc" % (ip))
            cmd = "scp -o StrictHostKeyChecking=no /etc/lib/swift/BootScripts/rc root@%s:/etc/init.d/rc" % (ip)
            (status, stdout, stderr) = sshpass(password, cmd, timeout=60)
            if status != 0:
                raise SshpassError(stderr)

        except TimeoutError as err:
            blackList.append(ip)
            returncode += 1
            logger.error("Failed to execute \"%s\" in time" % (cmd))
            print "Failed to spread rc.local to %s" % ip
            continue
        except SshpassError as err:
            blackList.append(ip)
            returncode += 1
            logger.error("Failed to execute \"%s\" for %s" % (cmd, err))
            print "Failed to rc.local to %s" % ip
            continue

    return (returncode, blackList)


def stopAllServices():
    logger = getLogger(name="stopAllServices")

    stopDaemon("rsync")
    stopDaemon("memcached")
    os.system("swift-init all stop")


def stopDaemon(daemonName):
    logger = getLogger(name="stopDaemon")

    cmd = "/etc/init.d/%s stop" % daemonName
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (stdoutData, stderrData) = po.communicate()

    if po.returncode != 0:
        logger.error(stderrData)
    else:
        logger.info(stdoutData)

    return po.returncode


def jsonStr2SshpassArg(jsonStr):
    arg = jsonStr.replace(" ", "")
    arg = arg.replace("{", "\{")
    arg = arg.replace("}", "\}")
    arg = arg.replace("\"", '\\"')
    arg = "\'" + arg + "\'"
    return arg


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


class SshpassError(Exception):
    def __init__(self, errMsg):
        self.errMsg = errMsg

    def __str__(self):
        return self.errMsg


class TryLockError(Exception):
    def __str__(self):
        return "Failed to tryLock lockFile"


if __name__ == '__main__':
    #createRamdiskDirs()
    #print hostname2Ip("GDFGSDFSD")
    #print restartSwiftProxy()
    pass
