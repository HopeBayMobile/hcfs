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
    
    def backupToCloud(self):
        """
        """
        self._datetime = strftime("%Y%m%d%H%M", gmtime())
        self._tarFileName = '%s_gw_conf_backup.tar.gz' % self._datetime
        self.copyFile()
        self.tarFile()
        self.sendToCloud()

    def copyFile(self):
        """
        """
        if os.path.exists(TEMP_PATH):
            shutil.rmtree(TEMP_PATH)
        os.mkdir(TEMP_PATH)
        i = 1
        for file in self._fileList:
            if os.path.exists(file):
                statInfo = os.stat(file)
                user = pwd.getpwuid(statInfo.st_uid)[0]
                group = grp.getgrgid(statInfo.st_gid)[0]
                fileMode = stat.S_IMODE(statInfo.st_mode)
                print user, group, fileMode
                key = ''.join(['file', str(i)])
                self._metaData[key] = {'fname': os.path.basename(file), 'fpath': os.path.dirname(file), 'user': user, 'group': group, 'chmod': fileMode}
                shutil.copy2(file, TEMP_PATH)
                i = i + 1
            else:
                log.warning('File(%s) is not exist' % file)
        print self._metaData
        try:
            fd = open(''.join([TEMP_PATH, '/metadata.txt']), 'w+')
            fd.write(json.dumps(self._metaData))
            fd.close()
        except IOError as e:
            raise MetaDataError()

    def tarFile(self):
        os.system('tar -zcvf %s %s' % (self._tarFileName, TEMP_PATH))
        
    def sendToCloud(self):
        self._cloudObject.upload('config', self._tarFileName)

def main(argv = None):
    test = BackupToCloud()
    test.backupToCloud()
if __name__ == '__main__':
    main()
		
