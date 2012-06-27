#!/usr/bin/python
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# Swift client class can connect to swift and do swift operation

import common
import subprocess
from GatewayError import *

log = common.getLogger(name="class name: SwiftClient",
                       conf="/etc/delta/Gateway.ini")


class SwiftClient():
    """
    SwiftClient class is used for swift command.
    Usage: swiftClient = SwiftClient(url, login, password)
           swiftClient.upload(container, file)
           swiftClient.executeCommand(command)
    """
    def __init__(self, url=None, login=None, password=None):
        """
        Construct a SwiftClient from url, login and password
        @type url: string
        @param url: swift url, for example: 127.0.0.1:8080
        @type login: string
        @param login: user name which can access swift
        @type password: string
        @param password: password which can access swift
        """
        self._url = url
        self._login = login
        self._password = password
        if self._url is None:
            self._url = '172.16.228.53:8080'
        if self._login is None:
            self._login = 'andy:andy'
        if self._password is None:
            self._password = 'testpass'

    def executeCommand(self, command=None):
        """
        compose swift command
        @type command: string
        @param command: swift command. For example: stat, list,
                        upload, post download, delete
        """
        if command is None:
            command = 'stat'
        cmd = ''.join(['sudo swift -A https://', self._url, '/auth/v1.0 -U ',
                       self._login, ' -K ', self._password, ' ', command])
        print "swift command is"
        print cmd
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)
        (stdout, stderr) = po.communicate()
        po.wait()
        print [po.returncode, stdout, stderr]
        return [po.returncode, stdout, stderr]

    def upload(self, container, file):
        """
        swift upload function
        @type container: string
        @param container: you want to save in which container
        @type file: string
        @param file: the local file which want to send to swift
        """
        try:
            returnData = self.executeCommand('upload %s %s'
                                             % (container, file))
            if file[0] == '/':
                file = file[1:]
            if returnData[1].strip() == file:
                return True
            else:
                log.error('[0] swift upload fail: %s' % returnData[1])
                raise SwiftUploadError('swift upload fail - error message: %s'
                                       % returnData[1])
        except SwiftUploadError:
            raise
        except Exception as e:
            print e
            log.error('[0] exception message: %s' % str(e))
            raise SwiftCommandError()

    def delete(self, container, file):
        """
        swift delete function
        @type container: string
        @param container: you want to delete in which container
        @type file: string
        @param file: the file name in swift
        """
        try:
            if file[0] == '/':
                file = file[1:]
            returnData = self.executeCommand('delete %s %s'
                                             % (container, file))
            if returnData[1].strip() == file:
                return True
            else:
                log.error('[0] swift delete fail: %s' % returnData[1])
                raise SwiftDeleteError('swift delete fail - error message: %s'
                                       % returnData[1])
        except SwiftDeleteError:
            raise
        except Exception as e:
            print e
            log.error('[0] exception message: %s' % str(e))
            raise SwiftCommandError()


def main():
    swift = SwiftClient('172.16.228.53:8080', 'dcloud:dgateway', 'testpass')
    swift.executeCommand()

if __name__ == '__main__':
    main()
