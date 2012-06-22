#!/usr/bin/python
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# Swift client class can connect to swift and do swift operation

import os
import subprocess
from GatewayError import *

class SwiftClient():
    """
    """
    def __init__(self, url = None, login = None, password = None):
        self._url = url
        self._login = login
        self._password = password
        
        if self._url is None:
            self._url = '172.16.228.53:8080'
        if self._login is None:
            self._login = 'andy:andy'
        if self._password is None:
            self._password = 'testpass'
            
    def executeCommand(self, command = None):
        if command is None:
            command = 'stat'
        cmd = ''.join(['sudo swift -A https://', self._url, '/auth/v1.0 -U ',
            self._login, ' -K ', self._password, ' ', command])
        print "swift command is"
        print cmd
        po  = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
        (stdout, stderr) = po.communicate()
        po.wait()
        return [po.returncode, stdout, stderr]

    def upload(self, container, file):
        try:
            returnData = self.executeCommand('upload %s %s' %(container, file))
            if returnData[0] == 0:
                return True
            else:
                log.error('swift upload fail: %s' %returnData[2])
                raise SwiftUploadError('swift upload fail error message: %s' %returnData[2])
        except SwiftUploadError:
            raise
        except Exception:
            raise SwiftCommandError()

def main(argv = None):
    swift = SwiftClient('172.16.228.53:8080', 'dcloud:dgateway', 'testpass')
    swift.executeCommand()
        
if __name__ == '__main__':
    main()
