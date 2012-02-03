'''
Created on 2011/11/04

@authors: Rice, CW and Ken

Modified by CW on 2011/11/22
Modified by CW on 2011/11/29
'''

import sys
import os
import socket
import posixfile
import time
import json
import subprocess
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

#Self defined packages
from GlusterdCfg import GlusterdCfg
sys.path.append('../')
from Utility.GlusterfsLog import GlusterfsLog

#Third party packages
import paramiko


class GlusterfsVolumeCreator:
	def __init__(self, hostList = [], brickPrefix = '/disk', volType = 'replica', count = 3):
		self.__hostList = hostList
		self.__volType = volType
		self.__count = count
		self.__brickPrefix = brickPrefix

	def setHostList(self, hostList):
		self.__hostList = hostList

	def setBrickPrefix(self, brickPrefix):
		self.__brickPrefix = brickPrefix

	def setVolType(self, volType): 
		self.__volType = volType

	def setCount(self, count):
		self.__count = count

	def computeBrickList(self): 
		brickNameList = []

		if self.__volType != 'replica' and self.__volType != 'stripe':
			self.__count = 1

		for i in range(0, 2):
			for index, host in enumerate(self.__hostList):
				brickName = host + ":" + self.__brickPrefix + str(i+1) + " "
				brickNameList.append(brickName)

		for i in range(len(brickNameList) % self.__count):
			brickNameList.pop()
		
		return ''.join(brickNameList) 


class GlusterfsDeploy:
	def __init__(self, configFile, hostList = [], taskId = ""): #modified
	
		#TODO: make sure local hostname is in the hostList
		self.__configFile = configFile
		self.__hostList = hostList
		self.__taskId = taskId

		config = ConfigParser()
		config.readfp(open(configFile))

		self.__break = Decimal(config.get('interval', 'break'))
		self.__probeInterval = Decimal(config.get('interval', 'probe'))
		self.__cmdInterval = Decimal(config.get('interval', 'cmd'))
		self.__probeTimeout = Decimal(config.get('timeout', 'probe'))
		self.__cmdTimeout = Decimal(config.get('timeout', 'cmd'))
		self.__logFile = config.get('log', 'dir') + '/' + config.get('log', 'GlusterfsDeploy')
		self.__reportDir = config.get('report', 'dir')
		if taskId == "":
			taskId = "test"
		self.__taskId = taskId
		self.__report = self.__reportDir + '/' + taskId + '/report'

		self.__username = config.get('main', 'username')
		self.__password = config.get('main', 'password')
		self.__numOfHosts = len(self.__hostList)

		self.__cfgLog = GlusterfsLog(self.__reportDir, self.__report, self.__logFile)
		self.__cfgLog.touchFile(self.__logFile)
		self.__cfgLog.touchFile(self.__report)
		self.__cfgLog.initProgress()
		
	def __runSubTask(self, hostList, subTaskName, subTaskWeight, subTaskId):
		pid = os.fork()
		
		if pid == 0:
			self.__cfgLog.logEvent("Subtask %s start\n"%subTaskId)
			
			if subTaskName == "cleanGlusterdCfg":
				GlusterdCfg(self.__configFile, hostList, subTaskId).cleanGlusterdCfg()
			elif subTaskName == "stopGlusterd":
				GlusterdCfg(self.__configFile, hostList, subTaskId).stopGlusterd()
			elif subTaskName == "startGlusterd":
				GlusterdCfg(self.__configFile, hostList, subTaskId).startGlusterd()
			elif subTaskName == "peerProbe":
				GlusterfsDeploy(self.__configFile, hostList, subTaskId).peerProbe()
			else:
				self.__cfgLog.logError("Erroneous subtask name: %s\n", subTaskName)
				raise 
 	
			self.__cfgLog.logEvent("Subtask %s end\n" % subTaskId)
			os._exit(0)

		oriProgress = self.__cfgLog.readProgress(self.__taskId)
		while True:
			report = self.__cfgLog.readReport(subTaskId)
			if report is None:
				time.sleep(0.1)
				continue
			progress = oriProgress + Decimal(report['progress']) * Decimal(subTaskWeight)
			self.__cfgLog.logProgress(progress)
			if report['finished'] == True:
				break
			time.sleep(0.1)

		return report

	def peerProbe(self):
		blackList = []
		count = 0
		progress = 0

		self.__cfgLog.logEvent("peer probe start\n")
		for host in self.__hostList:
			progress = Decimal(count) / Decimal(self.__numOfHosts)
			count = count + 1
			self.__cfgLog.logProgress(progress)

			elapsedTime = 0
			po = subprocess.Popen(["sudo", "gluster", "peer", "probe", host])
			while elapsedTime <= self.__probeTimeout:
				po.poll()
				if po.returncode is None:
					time.sleep(self.__probeInterval)
					elapsedTime = elapsedTime + self.__probeInterval
					continue
				break

			if (po.returncode != 0) or (po.returncode is None):
				if po.returncode is None:
					#Note: Will kill othe process who has the same id as the died child
					os.system("sudo kill -9 %s"%po.pid)
					#Note: Will this command fail>
					os.system("sudo gluster peer detach %s" % host)

                                blackList.append(host)
				errMsg = "Failed to probe %s\n" % host
				print >> sys.stderr, errMsg
                                self.__cfgLog.logError(errMsg)
                        else:
                                time.sleep(self.__probeInterval)

		#TODO: Parse and check every host in gluster peer status is in connection
		self.__cfgLog.logProgress(progress = 1, finished = True, code = len(blackList), blackList = blackList)
		self.__cfgLog.logEvent("peer probe end\n")

	def __runCommand(self, cmd, weight, errCode, blackList):
		self.__cfgLog.logEvent("Execution of \"%s\" start\n" % cmd)
		elapsedTime = 0
		errMsg = ""
		#po = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT): modified
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		while elapsedTime <= self.__cmdTimeout:
			po.poll()
			if po.returncode is None:
				time.sleep(self.__cmdInterval)
				elapsedTime = elapsedTime + self.__cmdInterval
				continue
			break

		if po.returncode is None:
			os.system("sudo kill -9 %s" % po.pid)
			errMsg = "Execution of \"%s\" failed: timeout!\n" % cmd 
		elif po.returncode != 0:
			errMsg = "Execution of \"%s\" failed: " % cmd + po.stdout.read()

		progress = self.__cfgLog.readProgress(self.__taskId)
		if errMsg != "":
			print >> sys.stderr, errMsg
                	self.__cfgLog.logError(errMsg)
			self.__cfgLog.logProgress(progress = progress, finished = True, code = errCode, blackList = blackList)
			return 1
		else:
			self.__cfgLog.logEvent("Execution of \"%s\" end\n" % cmd)
			self.__cfgLog.logProgress(progress = Decimal(progress) + Decimal(weight))
			return 0
	

	def volumeCreate(self, volCreator = GlusterfsVolumeCreator(), volName = 'testVol', brickPrefix = '/exp', volType = 'replica', count = 3, transport = 'tcp'):
		self.__cfgLog.logEvent("volumeCreate start\n")
		
		#TODO: write a function to Check if the input variables are correct

		hostSet = set(self.__hostList)
		blackSet = set()
		subTaskList = [('stopGlusterd', 0.1), ('cleanGlusterdCfg', 0.1), ('startGlusterd', 0.1), ('peerProbe', 0.3)]
		
		#TODO: How to deal with nodes which are alive but their glusterd cannot be stopped 
		for i in range(0, len(subTaskList)):
			subTaskId = self.__taskId + '/' + subTaskList[i][0]
			report = self.__runSubTask(self.__hostList, subTaskName = subTaskList[i][0], subTaskWeight = subTaskList[i][1], subTaskId = subTaskId)
			blackSet.update( set(report['blackList']))
			time.sleep(self.__break)
		
		volCreator.setHostList(list(hostSet-blackSet))
		volCreator.setVolType(volName)
		volCreator.setCount(count)
		volCreator.setBrickPrefix(brickPrefix)
		
		#Note: errCode = -2 indicate no enough hosts to create a volume
		brickNames = volCreator.computeBrickList()
		if len(brickNames) == 0:
			self.__cfgLog.logProgress(progress = 0.6, finished = True, code = -2, blackList = list(blackSet))
			self.__cfgLog.logError("volumeCreate failed: no enough hosts to create a new volume\n")
			return None

		#Note: errCode = -3 indicate creating a new vol failed
		cmd="sudo gluster volume create "
		if volType == 'replica' or volType == 'stripe':
			cmd = cmd + volName + " " + volType + " " + str(count) + " transport " + transport + " " + brickNames
		else:
			cmd = cmd + volName + " transport " + transport + " " + brickNames
		ret = self.__runCommand(cmd, 0.3, -3, list(blackSet))
		progress = self.__cfgLog.readProgress(self.__taskId)
		if ret != 0:
			self.__cfgLog.logError("volumeCreate failed: failed to create a new volume\n")
			return None
		
		#Note: errCode = -4 indicate starting vol failed
		cmd = "sudo gluster volume start " + volName
		ret = self.__runCommand(cmd, 0.1, -4, list(blackSet))
		if ret != 0:
			self.__cfgLog.logError("volumeCreate faled: failed to start the volume\n")
			return None

			
		mountPoint = socket.gethostname() + ":/" + volName

		self.__cfgLog.logProgress(progress = 1, finished = True, code = len(blackSet), blackList = list(blackSet), outcome = mountPoint)#modified
		self.__cfgLog.logEvent("volumeCreate end\n")
		return 0
		
	def volumeDelete(self, dataPreserved = True):

		self.__cfgLog.logEvent("volumeDelete start\n")
		
		hostSet = set(self.__hostList)
		blackSet = set()

		if dataPreserved == False:
			#TODO: remove all data in the volume
			pass
		else:	
			#TODO: How to deal with nodes which are alive but their glusterd cannot be stopped 
			subTaskId = self.__taskId + '/stopGlusterd'
			report = self.__runSubTask(self.__hostList, subTaskName = 'stopGlusterd', subTaskWeight = 0.5, subTaskId = subTaskId)
			blackSet.update(set(report['blackList']))
		
			subTaskId = self.__taskId + '/cleanGlusterdCfg'
			report = self.__runSubTask(self.__hostList, subTaskName = 'cleanGlusterdCfg', subTaskWeight = 0.5, subTaskId = subTaskId)
			blackSet.update(set(report['blackList']))

		self.__cfgLog.logProgress(progress = 1, finished = True, code = len(blackSet), blackList = list(blackSet))
		self.__cfgLog.logEvent("volumeDelete end\n")
		
		return len(blackSet)

	def renewReport(self):
		'''
                Clear the directroy containing the report and create an new empty report
                '''
		self.__cfgLog.initReport()


if __name__ == '__main__':
	
	GD= GlusterfsDeploy(configFile="../DCloudGfs.ini",hostList=['ntu01', 'ntu02', 'ntu09', 'ntu03'])
	GD.renewReport()
	print GD.volumeCreate()
	#GD.renewReport()
	#GD.volumeDelete()

