'''
Created on 2011/11/04

@authors: Rice, CW and Ken

Modified by CW on 2011/11/22
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

#Self defined packages
sys.path.append('../')
from Utility.GlusterfsLog import GlusterfsLog


class GlusterdCfg:
	def __init__(self, configFile, hostList = [], taskId = ""):
	
		self.__configFile = configFile
		self.__hostList = hostList
		self.__taskId = taskId

		config = ConfigParser()
		config.readfp(open(configFile))

		self.__logFile = config.get('log', 'dir') + '/' + config.get('log', 'GlusterdCfg')
		self.__reportDir = config.get('report', 'dir')
		if taskId == "":
			taskId = "test"
		self.__report = self.__reportDir + '/' + taskId + '/report'

		self.__username = config.get('main', 'username')
		self.__password = config.get('main', 'password')
		self.__numOfHosts = len(self.__hostList)

		self.__cfgLog = GlusterfsLog(self.__reportDir, self.__report, self.__logFile)
		self.__cfgLog.touchFile(self.__logFile)
		self.__cfgLog.touchFile(self.__report)
		self.__cfgLog.initProgress()
		
	def __execute(self, cmd):
		'''
		execute cmd for each host in hostList
		@cmd: command to execute
		'''
		ssh = paramiko.SSHClient()
		ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

                blackList = []
                count = 0
		errMsg = ""

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
					print "Succeed to run \"%s\"  on %s: \n%s" % (cmd, hostname, chan.recv(9999))

			except (paramiko.SSHException, paramiko.AuthenticationException, socket.error) as err:
                                errMsg = "Failed to run \"%s\" on %s: \n%s\n" % (cmd, hostname, str(err))
			finally:
				if errMsg != "":
					self.__cfgLog.logError(errMsg)
					print >> sys.stderr, errMsg
                                	blackList.append(hostname)

			count = count + 1
			progress = Decimal(count) / Decimal(self.__numOfHosts)
			self.__cfgLog.logProgress(progress)
			ssh.close()

		self.__cfgLog.logProgress(progress = 1, finished = True, code = len(blackList), blackList = blackList)

	def cmdDecorator(fn):
		def wrapper(*args):
			args[0].__cfgLog.logEvent(fn.__name__ + " start\n")
			fn(args[0])
			args[0].__cfgLog.logEvent(fn.__name__ + " end\n")
		return wrapper

	@cmdDecorator
	def startGlusterd(self):
		self.__execute("sudo /etc/init.d/glusterfs-server start")

	@cmdDecorator	
	def stopGlusterd(self):
		self.__execute("sudo /etc/init.d/glusterfs-server stop")

	@cmdDecorator
	def cleanGlusterdCfg(self):
		self.__execute("sudo rm -rf /etc/glusterd/*")

	#TODO: modify command
	@cmdDecorator
	def uninstallGlusterd(self):
		cmd = "sudo apt-get uninstall glusterfs; sudo rm -rf /etc/glusterd/"
		self.__execute(cmd)

	#TODO: modify command
	@cmdDecorator
	def installGlusterd(self):
		cmd = "sudo apt-get install glusterfs"
		self.__execute(cmd)

	@cmdDecorator
	def checkGlusterd(self):
		cmd = "sudo glusterfs --version"
		self.__execute(cmd)

	#@cmdDecorator
	#def diskClearing(self):
	#	cmd = "./DiskClearing.sh"
	#	self.__execute(cmd)


if __name__ == '__main__':
	GG = GlusterdCfg("../DCloudGfs.ini", ['ntu09', 'ntu06', 'ntu08', 'ntu07'])
	GG.stopGlusterd()
	GG.cleanGlusterdCfg()
	GG.startGlusterd()
	GG.checkGlusterd()

