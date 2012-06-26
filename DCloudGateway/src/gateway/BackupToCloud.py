#!/usr/bin/python
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# backup gateway configuration from gateway to cloud

import os
import json
import shutil
import grp
import pwd
import stat
import common
import simplejson as json
from time import gmtime, strftime
from SwiftClient import *

TEMP_PATH = '/tmp/backuptocloud'
log = common.getLogger(name="class name: BackupToCloud",
                       conf="/etc/delta/Gateway.ini")


class BackupToCloud():
    """
    This class implemenet backup process from local disk to cloud.
    Usage: backupObj = BackupToCloud(fileList, cloudObject)
           backupObj.backup()
    """
    def __init__(self, fileList=None, cloudObject=None):
        """
        @type fileList: list type
        @param fileList: file list which want to be sent to cloud
        @type cloudObject: string
        @param cloudObject: cloud object which can access cloud storage
        """
        self._fileList = fileList
        self._cloudObject = cloudObject
        if self._fileList is None:
            self._fileList = ['/etc/network/interfaces',
                              '/home/hungic/.bashrc', '/tmp/aaa']
            log.info('[3] input file list: %s' % self._fileList)
            #print self._fileList
        if self._cloudObject is None:
            self._cloudObject = SwiftClient()
        self._metaData = {}
        self.currentPath = os.getcwd()

    def backup(self):
        """
        start backup file list to cloud
        """
        log.info('[3] start backup config')
        self._datetime = strftime("%Y%m%d%H%M", gmtime())
        self._tarFileName = '%s_gw_conf_backup.tar.gz' % self._datetime
        self.copyFile()
        log.info('[3] prepare tar compression file')
        self.tarFile()
        log.info('[3] start send to cloud')
        self.sendToCloud()

    def copyFile(self):
        """
        copy file from original folder to temp folder
        and also save the user, group, permission information in metdata.txt
        """
        if os.path.exists(TEMP_PATH):
            shutil.rmtree(TEMP_PATH)
        os.mkdir(TEMP_PATH)
        i = 1
        for filename in self._fileList:
            if os.path.exists(filename):
                statInfo = os.stat(filename)
                user = pwd.getpwuid(statInfo.st_uid)[0]
                group = grp.getgrgid(statInfo.st_gid)[0]
                fileMode = oct(stat.S_IMODE(statInfo.st_mode))
                #print user, group, fileMode
                key = ''.join(['file', str(i)])
                self._metaData[key] = {'fname': os.path.basename(filename),
                                       'fpath': os.path.dirname(filename),
                                       'user': user, 'group': group,
                                       'chmod': fileMode}
                shutil.copy2(filename, TEMP_PATH)
                i = i + 1
            else:
                log.warning('[1] File(%s) is not exist' % filename)
        #print self._metaData
        try:
            with open(''.join([TEMP_PATH, '/metadata.txt']), 'w+') as fd:
                fd.write(json.dumps(self._metaData))
        except:
            raise CreateMetaDataError()

    def tarFile(self):
        """
        tar files and metadata.txt
        """
        os.system('cd %s;tar -zcvf %s ./ --exclude *.tar.gz'
                  % (TEMP_PATH, self._tarFileName))

    def sendToCloud(self):
        """
        send tar.gz file to cloud storage
        """
        os.chdir(TEMP_PATH)
        self._cloudObject.upload('config', self._tarFileName)


def main():
    test = BackupToCloud()
    test.backup()
if __name__ == '__main__':
    main()
