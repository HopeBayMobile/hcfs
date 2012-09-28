import os
import subprocess
import threading
import sys
import pickle
import socket
import util
import re

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))

#TODO: Read from config files
UNNECESSARYFILES = "cert* backups *.conf"


class MountSwiftDeviceError(Exception):
    pass


class WriteMetadataError(Exception):
    pass


def getRootDisk():
    cmd = "mount"
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()

    device = lines[0].split()[0]
    return device[:8]


def getAllDisks():
    cmd = "fdisk -l"
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()

    disks = []
    for line in lines:
        match = re.match(r"^Disk /dev/sd\w:", line)
        if match is not None:
            disks.append(line.split()[1][:8])

    return disks


def getNonRootDisks():
    rootDisk = getRootDisk()
    disks = getAllDisks()
    nonRootDisks = []

    for disk in disks:
        if disk != rootDisk:
            nonRootDisks.append(disk)

    return nonRootDisks


def getMountedDisks():
    cmd = "mount"
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    mountedDisks = set()

    for line in lines:
        disk = line.split()[0]
        if disk.startswith("/dev/sd"):
            mountedDisks.add(disk[0: 8])

    return mountedDisks


def getUmountedDisks():
    disks = getNonRootDisks()
    mountedDisks = getMountedDisks()
    umountedDisks = set()
    for disk in disks:
        if not disk in mountedDisks:
            umountedDisks.add(disk)

    return umountedDisks


def getUnusedDisks(vers):
    disks = getUmountedDisks()
    unusedDisks = []
    for disk in disks:
        (ret, fingerprint) = readFingerprint(disk)
        if ret != 0 or not util.isValid(vers, fingerprint):
            unusedDisks.append(disk)

    return unusedDisks


def getMountedSwiftDevices(devicePrx):
    cmd = "mount"
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    prefix = "/srv/node/%s" % devicePrx

    mountedSwiftDevices = dict()

    for line in lines:
        disk = line.split()[0]
        mountpoint = line.split()[2]
        if mountpoint.startswith(prefix):
            deviceNum = int(mountpoint.replace(prefix, ""))
            mountedSwiftDevices.setdefault(deviceNum, disk)

    return mountedSwiftDevices


def getUmountedSwiftDevices(deviceCnt, devicePrx):
    '''
    get a set of unmounted swift devices' numbers
    @type deviceCnt: integer
    @param deviceCnt: expected number of swift devices
    @type devicePrx: string
    @param  devicePrx: prefix of swift devices' mountpoints
    @rtype: set of integers
    @return: set of umounted swift devices' numbers
    '''

    if not deviceCnt:
        return set()

    cmd = "mount"
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    po.wait()

    prefix = "/srv/node/%s" % devicePrx
    devices = set(range(1, deviceCnt + 1))

    for line in lines:
        disk = line.split()[0]
        mountpoint = line.split()[2]
        if mountpoint.startswith(prefix):
            deviceNum = int(mountpoint.replace(prefix, ""))
            devices.discard(deviceNum)

    return devices


def formatDisks(diskList):
    logger = util.getLogger(name="formatDisks")
    returncode = 0
    formattedDisks = []

    for disk in diskList:
        cmd = "mkfs.xfs -f -i size=1024 %s" % (disk)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
            logger.error("Failed to format %s for %s" % (disk, output))
            returncode += 1
            continue

        formattedDisks.append(disk)

    return (returncode, formattedDisks)


def formatNonRootDisks(deviceCnt=1):
    '''
    Format deviceCnt non-root disks
    '''
    logger = util.getLogger(name="formatNonRootDisks")
    disks = getNonRootDisks()
    formattedDisks = []
    returncode = 0

    if not deviceCnt:
        return (returncode, formattedDisks)

    for disk in disks:
        if len(formattedDisks) == deviceCnt:
            break

        cmd = "mkfs.xfs -f -i size=1024 %s" % (disk)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
            logger.warn("Failed to format %s for %s" % (disk, output))
            continue

        formattedDisks.append(disk)

    returncode = 0 if deviceCnt - len(formattedDisks) == 0 else 0

    return (returncode, formattedDisks)


def mountDisk(disk, mountpoint):
    logger = util.getLogger(name="mountDisk")
    logger.info("mountDisk start")

    returncode = 0
    try:
        os.system("mkdir -p %s" % mountpoint)
        if os.path.ismount(mountpoint):
            os.system("umount -l %s" % mountpoint)

        #TODO: Add timeout mechanism
        cmd = "mount -t xfs -o noatime,nodiratime,nobarrier,logbufs=8 %s %s" % (disk, mountpoint)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
            logger.error("Failed to mount  %s for %s" % (disk, output))
            returncode = 1

    except OSError as e:
        logger.error("Failed to mount %s for %s" % (disk, e))
        returncode = 1
    finally:
        logger.info("mountDisk end")
        return returncode


def mountSwiftDevice(disk, devicePrx, deviceNum):
    logger = util.getLogger(name="mountSwiftDevice")

    mountpoint = "/srv/node/%s%d" % (devicePrx, deviceNum)
    returncode = mountDisk(disk, mountpoint)

    if returncode != 0:
        logger.error("Failed to mount %s on %s" % (disk, mountpoint))

    # prepare objects dir to fix bugs of swift rsync errors
    if not os.path.exists("%s/objects" % mountpoint):
        os.system("mkdir %s/objects" % mountpoint)
        os.system("chown swift:swift %s/objects" % mountpoint)

    return returncode


def mountUmountedSwiftDevices():
    logger = util.getLogger(name="mountUmountedSwiftDevices")

    vers = util.getSwiftConfVers()
    devicePrx = util.getDevicePrx()
    deviceCnt = util.getDeviceCnt()

    umountedSwiftDevices = getUmountedSwiftDevices(deviceCnt=deviceCnt, devicePrx=devicePrx)

    disks = getUmountedDisks()

    for disk in disks:
        (ret, fingerprint) = readFingerprint(disk)
        if ret != 0:
            logger.info("Failed to read fingerprint from disk %s" % disk)
            continue

        deviceNum = fingerprint["deviceNum"]
        if util.isValid(vers, fingerprint) and deviceNum in umountedSwiftDevices:
            if mountSwiftDevice(disk=disk, devicePrx=devicePrx, deviceNum=deviceNum) == 0:
                umountedSwiftDevices.discard(deviceNum)

    return umountedSwiftDevices


def cleanMetadata(disk):
    logger = util.getLogger(name="cleanFingerprint")
    mountpoint = "/tmp/%s" % disk

    ret = 1
    if mountDisk(disk, mountpoint) != 0:
        logger.error("Failed to mount %s" % disk)
        return ret

    try:
        ret = os.system("rm -rf %s/fingerprint" % mountpoint)
        ret += os.system("rm -rf %s/swift" % mountpoint)
        ret += os.system("rm -rf %s/DCloudSwift" % mountpoint)

    except Exception as e:
        logger.error("Failed to clean fingerprint for %s" % str(e))

    finally:
        if lazyUmount(mountpoint) != 0:
            logger.warn("Failed to umount disk %s from %s" % (disk, mountpoint))

        return ret


def cleanMetadataOnDisks():
    logger = util.getLogger(name="cleanMetadata")
    logger.info("start")

    disks = getNonRootDisks()
    returncode = 0

    for disk in disks:
        if cleanMetadata(disk) != 0:
            returncode += 1

    return returncode

    logger.info("end")


def createLostSwiftDevices(lostDevices):
    logger = util.getLogger(name="createLostSwiftDevices")
    logger.info("start")

    vers = util.getSwiftConfVers()
    deviceCnt = util.getDeviceCnt()
    devicePrx = util.getDevicePrx()

    disks = getUnusedDisks(vers)
    (ret, disks) = formatDisks(disks)

    mLostDevices = set(lostDevices)

    for disk in disks:
        try:
            if len(mLostDevices) == 0:
                break
            deviceNum = mLostDevices.pop()
            mountpoint = "/srv/node" + "/%s%d" % (devicePrx, deviceNum)
            os.system("mkdir -p %s" % mountpoint)
            if os.path.ismount(mountpoint):
                os.system("umount -l %s" % mountpoint)

            print "%s\n" % mountpoint

            if writeMetadata(disk=disk, vers=vers, deviceCnt=deviceCnt, devicePrx=devicePrx, deviceNum=deviceNum) != 0:
                raise WriteMetadataError("Failed to write fingerprint into %s" % disk)

            if mountSwiftDevice(disk=disk, devicePrx=devicePrx, deviceNum=deviceNum) != 0:
                raise MountSwiftDeviceError("Failed to mount %s on %s" % (disk, mountpoint))

        except WriteMetadataError as err:
            logger.error("%s" % err)
            mLostDevices.add(deviceNum)
            continue
        except MountSwiftDeviceError as err:
            logger.error("%s" % err)
            continue

    os.system("mkdir -p /srv/node")
    os.system("find /srv/node -maxdepth 1 -exec sudo chown swift:swift '{}' \;")
    logger.info("end")
    return mLostDevices


def createSwiftDevices(deviceCnt=3, devicePrx="sdb"):
    logger = util.getLogger(name="createSwiftDevices")
    logger.debug("start")

    lazyUmountSwiftDevices()
    (ret, disks) = formatNonRootDisks(deviceCnt)
    if ret != 0:
        return deviceCnt

    #TODO: modified to match the ring version
    vers = util.getSwiftConfVers()
    count = 0
    for disk in disks:
        try:
            count += 1
            mountpoint = "/srv/node" + "/%s%d" % (devicePrx, count)
            os.system("mkdir -p %s" % mountpoint)
            if os.path.ismount(mountpoint):
                os.system("umount -l %s" % mountpoint)

            print "%s\n" % mountpoint

            if writeMetadata(disk=disk, vers=vers, deviceCnt=deviceCnt, devicePrx=devicePrx, deviceNum=count) != 0:
                raise WriteMetadataError("Failed to write fingerprint into %s" % disk)

            if mountSwiftDevice(disk=disk, devicePrx=devicePrx, deviceNum=count) != 0:
                raise MountSwiftDeviceError("Failed to mount %s on %s" % (disk, mountpoint))

            if count == deviceCnt:
                break
        except WriteMetadataError as err:
            logger.error("%s" % err)
            count -= 1
            continue
        except MountSwiftDeviceError as err:
            logger.error("%s" % err)
            continue

    os.system("mkdir -p /srv/node")
    os.system("find /srv/node -maxdepth 1 -exec sudo chown swift:swift '{}' \;")

    logger.debug("end")
    return deviceCnt - count


def readFingerprint(disk):
    logger = util.getLogger(name="readFingerprint")
    fingerprint = {}
    mountpoint = "/tmp/%s" % disk
    os.system("mkdir -p %s" % mountpoint)

    #TODO: chechsum
    if mountDisk(disk, mountpoint) != 0:
        logger.debug("Failed to mount %s" % disk)
        return (1, fingerprint)

    try:
        with open("%s/fingerprint" % mountpoint, "rb") as fh:
            fingerprint = pickle.load(fh)

        return (0, fingerprint)
    except IOError as e:
        logger.debug("Failed to read fingerprint from %s for %s" % (disk, e))
        return (1, fingerprint)
    finally:
        if lazyUmount(mountpoint) != 0:
            logger.warn("Failed to umount disk %s from %s" % (disk, mountpoint))


def getLatestFingerprint():
    logger = util.getLogger(name="getLatestFingerprint")
    logger.debug("getLatestFingerprint start")
    disks = getNonRootDisks()
    latestFingerprint = None

    for disk in disks:
        (ret, fingerprint) = readFingerprint(disk)
        if ret == 0:
            if latestFingerprint is None or latestFingerprint["vers"] < fingerprint["vers"]:
                    latestFingerprint = fingerprint

    logger.debug("getLatestFingerprint end")
    return latestFingerprint


def getLatestVers():
    logger = util.getLogger(name="getLatestVers")
    logger.info("start")
    disks = getNonRootDisks()
    latestVers = None

    for disk in disks:
        (ret, fingerprint) = readFingerprint(disk)
        if ret == 0:
            if latestVers is None or latestVers < fingerprint["vers"]:
                latestVers = fingerprint["vers"]

    logger.info("end")
    return latestVers


def __loadSwiftMetadata(disk):
    logger = util.getLogger(name="__loadSwiftMetadata")
    mountpoint = "/tmp/%s" % disk
    os.system("mkdir -p %s" % mountpoint)

    #TODO: chechsum
    if mountDisk(disk, mountpoint) != 0:
        logger.error("Failed to mount %s" % disk)
        return 1

    returncode = 0
    cmd = "cp %s/swift/* /etc/swift/" % (mountpoint)
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()

    if po.returncode != 0:
        logger.error("Failed to reload swift metadata from %s for %s" % (disk, output))
        returncode = 1

    if lazyUmount(mountpoint) != 0:
        logger.warn("Failed to lazy umount disk %s from %s" % (disk, mountpoint))

    return returncode


def loadSwiftMetadata():
    logger = util.getLogger(name="loadSwiftMetadata")
    logger.info("start")

    disks = getNonRootDisks()
    latestFingerprint = getLatestFingerprint()
    os.system("mkdir -p /etc/swift")
    returncode = 1

    for disk in disks:
        (ret, fingerprint) = readFingerprint(disk)
        if ret == 0 and fingerprint["vers"] >= latestFingerprint["vers"]:
            if __loadSwiftMetadata(disk) == 0:
                returncode = 0
                break

    os.system("chown -R swift:swift /etc/swift")
    logger.info("end")
    return returncode


def remountRecognizableDisks():
    logger = util.getLogger(name="remountRecognizableDisks")

    disks = getNonRootDisks()
    unusedDisks = []
    lostDevices = []

    latest = getLatestFingerprint()
    if latest is None:
        return (lostDevices, disks)

    seenDevices = set()
    for disk in disks:
        (ret, fingerprint) = readFingerprint(disk)
        if ret == 0 and fingerprint["hostname"] == socket.gethostname() and fingerprint["vers"] == latest["vers"] and fingerprint["deviceNum"] not in seenDevices:
            mountpoint = "/srv/node/%s%d" % (fingerprint["devicePrx"], fingerprint["deviceNum"])
            if mountSwiftDevice(disk=disk, devicePrx=fingerprint["devicePrx"], deviceNum=fingerprint["deviceNum"]) == 0:
                seenDevices.add(fingerprint["deviceNum"])
                print "/srv/node/%s%d is back!" % (fingerprint["devicePrx"], fingerprint["deviceNum"])
                continue
            else:
                logger.error("Failed to mount disk %s as swift device %s%d" % (disk, fingerprint["devicePrx"], fingerprint["deviceNum"]))

        unusedDisks.append(disk)

    lostDevices = [x for x in range(1, latest["deviceCnt"] + 1) if x not in seenDevices]
    return (lostDevices, unusedDisks)


def remountDisks():
    logger = util.getLogger(name="remountDisks")
    logger.info("start")

    latest = getLatestFingerprint()

    if latest is None:
        return (0, [])

    lazyUmountSwiftDevices()

    (lostDevices, unusedDisks) = remountRecognizableDisks()

    for disk in formatDisks(unusedDisks)[1]:
        if len(lostDevices) == 0:
            break
        if writeMetadata(disk=disk, vers=latest["vers"], deviceCnt=latest["deviceCnt"], devicePrx=latest["devicePrx"], deviceNum=lostDevices[0]) == 0:
            deviceNum = lostDevices[0]
            if mountSwiftDevice(disk=disk, devicePrx=latest["devicePrx"], deviceNum=deviceNum) == 0:
                lostDevices.pop(0)
                print "/srv/node/%s%d is back!" % (latest["devicePrx"], deviceNum)
            else:
                logger.error("Failed to mount disk %s as swift device %s%d " % (disk, latest["devicePrx"], deviceNum))
        else:
            logger.error("Failed to write metadata to %s" % disk)

    logger.info("end")
    return (len(lostDevices), lostDevices)


def lazyUmount(mountpoint):
    logger = util.getLogger(name="umount")

    returncode = 0
    try:
        if os.path.ismount(mountpoint):
            cmd = "umount -l %s" % mountpoint
            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()

            if po.returncode != 0:
                logger.error("Failed to umount -l  %s for %s" % (mountpoint, output))
                returncode = 1

    except OSError as e:
        logger.error("Failed to umount -l %s for %s" % (mountpoint, e))
        returncode = 1

    return returncode


def lazyUmountSwiftDevices():
    cmd = "umount -l /srv/node/*"
    os.system(cmd)


def __loadScripts(disk):
    logger = util.getLogger(name="__loadSripts")
    metadata = {}
    mountpoint = "/tmp/%s" % disk
    os.system("mkdir -p %s" % mountpoint)

    #TODO: chechsum
    if mountDisk(disk, mountpoint) != 0:
        logger.error("Failed to mount %s" % disk)
        return 1

    returncode = 0

    cmd = "cp -r %s/DCloudSwift /" % (mountpoint)
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()

    po.wait()
    if po.returncode != 0:
        logger.error("Failed to reload scripts from %s for %s" % (disk, output))
        returncode = 1

    if lazyUmount(mountpoint) != 0:
        logger.warn("Failed to umount disk %s from %s" % (disk, mountpoint))

    return returncode


def loadScripts():
    logger = util.getLogger(name="loadSripts")
    logger.info("start")

    disks = getNonRootDisks()
    latestVers = getLatestVers()
    os.system("mkdir -p /etc/swift")
    returncode = 1

    for disk in disks:
        (ret, fingerprint) = readFingerprint(disk)
        if ret == 0 and fingerprint["vers"] >= latestVers:
            if __loadScripts(disk) == 0:
                returncode = 0
                break

    logger.info("end")
    return returncode


def dumpScripts(destDir):
    logger = util.getLogger(name="dumpScripts")
    os.system("mkdir -p %s" % destDir)
    os.system("rm -r %s/*" % destDir)
    cmd = "cp -r %s/DCloudSwift/* %s" % (BASEDIR, destDir)
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()

    if po.returncode != 0:
        logger.error("Failed to dump scripts to %s for %s" % (destDir, output))
        return 1

    return 0


def dumpSwiftMetadata(destDir):
    logger = util.getLogger(name="dumpSwiftMetadata")
    os.system("mkdir -p %s" % destDir)
    os.system("mkdir -p /etc/swift")
    cmd = "cp -r /etc/swift/* %s" % destDir
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()

    if po.returncode != 0:
        logger.error("Failed to dump swift metadata to %s for %s" % (destDir, output))
        return 1

    os.system("cd %s; rm -rf %s" % (destDir, UNNECESSARYFILES))
    return 0


def updateMountedSwiftDevices():
    logger = util.getLogger(name="updateMetadataOnMountedDevices")

    vers = util.getSwiftConfVers()
    devicePrx = util.getDevicePrx()
    deviceCnt = util.getDeviceCnt()

    mountedSwiftDevices = getMountedSwiftDevices(devicePrx)
    blackSet = set()
    for deviceNum in mountedSwiftDevices:
        disk = mountedSwiftDevices[deviceNum]
        ret = writeMetadata(disk=disk, vers=vers, deviceCnt=deviceCnt, devicePrx=devicePrx, deviceNum=deviceNum)
        if ret != 0:
            logger.error("Failed to update metadata on disk %s" % disk)
            blackSet.add(deviceNum)
            continue

        logger.info("Succeed to update Metadata on disk %s with deviceNum=%d and vers=%s" % (disk, deviceNum, vers))

    return blackSet


def updateUmountedSwiftDevices(oriVers):
    logger = util.getLogger(name="updateUmountedSwiftDevices")

    newVers = util.getSwiftConfVers()
    devicePrx = util.getDevicePrx()
    deviceCnt = util.getDeviceCnt()

    umountedSwiftDevices = getUmountedSwiftDevices(deviceCnt=deviceCnt, devicePrx=devicePrx)

    disks = getUmountedDisks()

    for disk in disks:
        (ret, fingerprint) = readFingerprint(disk)
        if ret != 0:
            logger.warn("Failed to read fingerprint from disk %s" % disk)
            continue

        deviceNum = fingerprint["deviceNum"]
        if util.isValid(oriVers, fingerprint) and deviceNum in umountedSwiftDevices:
            if writeMetadata(disk=disk, vers=newVers, deviceCnt=deviceCnt, devicePrx=devicePrx, deviceNum=deviceNum) == 0:
                logger.info("Succeed to update metadata on disk %s with deviceNum=%d and vers=%s" % (disk, deviceNum, newVers))
                umountedSwiftDevices.discard(deviceNum)

    return umountedSwiftDevices


def updateMetadataOnDisks(oriVers):
    logger = util.getLogger(name="updateMetadataOnDisks")

    blackSet = updateMountedSwiftDevices()
    lostDevices = updateUmountedSwiftDevices(oriVers=oriVers)
    lostDevices = createLostSwiftDevices(lostDevices)
    return lostDevices.union(blackSet)


def writeMetadata(disk, vers, deviceCnt, devicePrx, deviceNum):
    logger = util.getLogger(name="writeMetadata")

    mountpoint = "/tmp/%s" % disk
    os.system("mkdir -p %s" % mountpoint)
    if os.path.ismount(mountpoint):
        os.system("umount -l %s" % mountpoint)

    cmd = "mount %s %s" % (disk, mountpoint)
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()

    if po.returncode != 0:
        logger.error("Failed to mount %s for %s" % (disk, output))
        return po.returncode

    #TODO: write checksum
    os.system("touch /%s/fingerprint" % mountpoint)
    fingerprint = {"hostname": socket.gethostname(), "vers": vers, "deviceCnt": deviceCnt, "devicePrx": devicePrx, "deviceNum": deviceNum}

    try:
        with open("%s/fingerprint" % mountpoint, "wb") as fh:
            pickle.dump(fingerprint, fh)

        if dumpSwiftMetadata("/%s/swift" % mountpoint) != 0:
            logger.error("Failed to dump swift metadata to %s" % (disk))
            return 1

        if dumpScripts("/%s/DCloudSwift" % mountpoint) != 0:
            logger.error("Failed to dump scripts to %s" % (disk))
            return 1

        return 0
    except IOError as e:
        logger.error("Failed to wirte fingerprint for disk %s" % disk)
        return 1
    finally:
        if lazyUmount(mountpoint) != 0:
            logger.warn("Failed to lazy umount disk %s from %s" % (disk, mountpoint))


if __name__ == '__main__':
    pass
    print cleanMetadataOnDisks()
    #print getUmountedDisks()
    #print getUmountedSwiftDevices(deviceCnt=5, devicePrx="sdb")
    #util.generateSwiftConfig()
    #formatDisks(["/dev/sdc"])
    #print getRootDisk()
    #print getAllDisks()
    #print formatNonRootDisks(deviceCnt=3)
    #print getMajorityHostname()
    #print getLatestMetadata()
    #createSwiftDevices()
    #print updateMetadataOnDisks(2)
    #print updateMountedSwiftDevices(vers=2, deviceCnt=5, devicePrx="sdb")
    #writeMetadata(disk="/dev/sdb", vers=1, deviceNum=1, devicePrx="sdb", deviceCnt=5)
    #writeMetadata(disk="/dev/sdc", vers=1, deviceNum=2, devicePrx="sdb", deviceCnt=5)
    #writeMetadata(disk="/dev/sdd", vers=1, deviceNum=3, devicePrx="sdb", deviceCnt=5)
    #writeMetadata(disk="/dev/sde", vers=1, deviceNum=4, devicePrx="sdb", deviceCnt=5)
    #writeMetadata(disk="/dev/sdf", vers=0, deviceNum=5, devicePrx="sdb", deviceCnt=5)
    #print updateMetadataOnDisks(oriVers=1)
    #print "/dev/sdb %s"%str(readMetadata(disk="/dev/sdb"))
    #print "/dev/sdc %s"%str(readMetadata(disk="/dev/sdc"))
    #print "/dev/sdd %s"%str(readMetadata(disk="/dev/sdd"))
    #print "/dev/sde %s"%str(readMetadata(disk="/dev/sde"))
    #print "/dev/sdf %s"%str(readMetadata(disk="/dev/sdf"))
    #mountUmountedSwiftDevices()
    #print remountDisks()
