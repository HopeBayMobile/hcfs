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
import simplejson as json
from time import gmtime, strftime
from SwiftClient import *

TEMP_PATH = '/tmp/backuptocloud'

class BackupToCloud():
    """
	
    """
    def __init__(self, fileList = None, cloudObject = None):
        self._fileList = fileList
        self._cloudObject = cloudObject
        if self._fileList is None:
            self._fileList = ['/etc/network/interfaces', '/home/hungic/.bashrc']
            print self._fileList
        if self._cloudObject is None:
            self._cloudObject = SwiftClient()
        self._metaData = {}
    
    def backupToCloud(self):
        """
        """
        self._datetime = strftime("%Y%m%d%H%M", gmtime())
        self.copyFile()
        #self.tarFile()
        #self.sendToCloud()

    def copyFile(self):
        """
        """
        shutil.rmtree(TEMP_PATH)
        os.mkdir(TEMP_PATH)
        i = 1
        for file in self._fileList:
            statInfo = os.stat(file)
            user = pwd.getpwuid(statInfo.st_uid)[0]
            group = grp.getgrgid(statInfo.st_gid)[0]
            fileMode = stat.S_IMODE(statInfo.st_mode)
            print user, group, fileMode
            key = ''.join(['file', str(i)])
            self._metaData[key] = {'fname': os.path.basename(file), 'fpath': os.path.dirname(file), 'user': user, 'group': group, 'chmod': fileMode}
            shutil.copy(file, TEMP_PATH)
            i = i + 1
        print self._metaData
        try:
            print ''.join([TEMP_PATH, '/metadata.txt'])
            fd = open(''.join([TEMP_PATH, '/metadata.txt']), 'w+')
            fd.write(json.dumps(self._metaData))
            fd.close()
        except IOError as e:
            print e

    def tarFile(self):
        

def main(argv = None):
    test = BackupToCloud()
    test.backupToCloud()
if __name__ == '__main__':
    main()
		
