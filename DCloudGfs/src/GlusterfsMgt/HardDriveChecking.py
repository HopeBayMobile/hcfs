'''
Created on 2012/02/14

@authors: CW

Modified by CW on 2012/02/15
'''

import sys
import os
import socket
import posixfile
import json
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

#Third party packages
import paramiko


class HardDriveChecking:
	def __init__(self, hostList = []):
		self.__hostList = hostList

		config = ConfigParser()
                config.readfp(open("/etc/DCloud/DCloudGfs/src/DCloudGfs.ini"))

		self.__username = config.get('main', 'username')
                self.__password = config.get('main', 'password')

	def DiskTesting(self):
		'''
		test the existence of /dev/sda for each host in hostList
		If /dev/sda is not found, the host is dropped.
		'''
		ssh = paramiko.SSHClient()
		ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

		resultHostList = []
		errMsg = ""
		cmd = "sudo test -e /dev/sda"

		for hostname in self.__hostList[0:]:
			try:
				ssh.connect(hostname, username = self.__username, password = self.__password)
				chan = ssh.get_transport().open_session()
				chan.exec_command(cmd)
				status = chan.recv_exit_status()
				errMsg = ""
				
				if status != 0:
					errMsg = "Failed to run \"%s\" on %s with exit status %s: \n%s" % (cmd, hostname, status, chan.recv_stderr(9999))
				else:
					resultHostList.append(hostname)
					print "Succeed to run \"%s\"  on %s: \n%s" % (cmd, hostname, chan.recv(9999))

			except (paramiko.SSHException, paramiko.AuthenticationException, socket.error) as err:
                                errMsg = "Failed to run \"%s\" on %s: \n%s\n" % (cmd, hostname, str(err))
			finally:
				if errMsg != "":
					print >> sys.stderr, errMsg

			ssh.close()

		return resultHostList


if __name__ == '__main__':
	GG = GlusterdCfg("../DCloudGfs.ini", ['ntu09', 'ntu06', 'ntu08', 'ntu07'])
	GG.stopGlusterd()

