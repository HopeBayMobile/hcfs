#!/usr/bin/env python
import sys
import subprocess
import os
import json
import urllib
import re

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.insert(0,"%s/DCloudSwift/" % BASEDIR)

from util import util

class DiskInfo:

    def __init__(self):
        pass

    def get_mount_point(self, disk):
        """
        check if the given disk is healthy
        @type  disk: string
        @param disk: name of the disk to get mount point
        @return: mount point of disk in /srv/node/
        """
        cmd = "sudo mount"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()

        for line in lines:
                match = re.match(r"^%s" % disk, line)
                if match is not None:
                        mountpoint = line.split()[2]
                        if mountpoint.startswith("/srv/node"):
                            return mountpoint.strip()

        return None

    def get_all_disks(self):
        cmd = "sudo smartctl --scan"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()

        disks = []
        for line in lines:
                match = re.match(r"^/dev/sd\w", line)
                if match is not None:
                        disks.append(line.split()[0][:8])

        return disks

    def is_healthy(self, disk):
        """
        check if the given disk is healthy
        @type  disk: string
        @param disk: name of the disk to check health
	@rtype: bool 
        @return: True iff the disk is healthy
        """

        cmd ="sudo smartctl -H %s" % disk
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        output = output.lower()
        if output.find("smart overall-health self-assessment test result: passed") != -1:
            return True
        else:
            return False



    def lazy_umount_broken_disks(self):
        """
        lazy umount broken disks from /srv/node/
        """
        logger = util.getLogger(name="DiskInfo.lazy_umount_broken_disks")
        logger.info("start")
        disks = self.get_all_disks()

        for disk in disks:
            if not is_healthy(disk):
                mountpoint = self.get_mount_point(disk)
                if mountpoint:
                    cmd ="sudo umount -l %s" % disk
                    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    lines = po.stdout.readlines()
                    po.wait()
                    if po.returncode != 0:
                      
                    po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                    output = po.stdout.read()
                    po.wait()

if __name__ == '__main__':
    '''
    main()
    '''
    DI = DiskInfo()
    mountpoint = DI.get_mount_point("/dev/sda")
    print mountpoint
