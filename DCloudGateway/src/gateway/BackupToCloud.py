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

TEMP_PATH = './temp'
log = common.getLogger(name="Backup To Cloud", conf="/etc/delta/Gateway.ini")

class BackupToCloud():
    """
    """
    def __init__(self, fileList = None, cloudObject = None):
        self._fileList = fileList
        self._cloudObject = cloudObject
        if self._fileList is None:
            self._fileList = ['/etc/network/interfaces', '/home/hungic/.bashrc', '/tmp/aaa']
            print self._fileList
        if self._cloudObject is None:
            self._cloudObject = SwiftClient()
        self._metaData = {}
    
    def backup(self):
        """
        """
        log.info('start backup config')
        self._datetime = strftime("%Y%m%d%H%M", gmtime())
        self._tarFileName = '%s_gw_conf_backup.tar.gz' % self._datetime
        self.copyFile()
        log.info('prepare tar compression file')
        self.tarFile()
        log.info('start send to cloud')
        self.sendToCloud()

    def copyFile(self):
        """
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
                fileMode = stat.S_IMODE(statInfo.st_mode)
                key = ''.join(['file', str(i)])
                self._metaData[key] = {'fname': os.path.basename(filename), 
                                       'fpath': os.path.dirname(filename), 
                                       'user': user, 'group': group, 
                                       'chmod': fileMode}
                shutil.copy2(filename, TEMP_PATH)
                i = i + 1
            else:
                log.warning('File(%s) is not exist' % filename)
        #print self._metaData
        try:
            with open(''.join([TEMP_PATH, '/metadata.txt']), 'w+') as fd:
                fd.write(json.dumps(self._metaData))
        except:
            raise CreateMetaDataError()

    def tarFile(self):
        """
        """
        os.system('cd %s;tar -zcvf %s ./ --exclude *.tar.gz; mv %s ../' 
                  % (TEMP_PATH, self._tarFileName, self._tarFileName))
        
    def sendToCloud(self):
        """
        """
        returnData = self._cloudObject.upload('config', self._tarFileName)


def main():
    test = BackupToCloud()
    test.backup()
if __name__ == '__main__':
    main()

