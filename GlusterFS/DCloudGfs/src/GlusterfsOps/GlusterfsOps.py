'''
Created on 2011/11/13

@authors: Rice, CW and Ken

Modified by CW on 2011/11/23
'''

import sys
import os
import socket
import posixfile
import time
import json
import subprocess
import shlex
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser


#Third party packages
import paramiko

#Self defined packages
sys.path.append('../')
from Utility.GlusterfsLog import GlusterfsLog
#from Utility.ParsePeerStatus import ParsePeerStatus


class GlusterfsOps:
	def __init__(self,ConfigFile, taskId=""): #modified
	
		#TODO: make sure local hostname is in the hostList
		self.__taskId = taskId

		config = ConfigParser()
		config.readfp(open(ConfigFile))

		self.__cmdInterval = Decimal(config.get('interval', 'cmd'))
		self.__cmdTimeout = Decimal(config.get('timeout', 'cmd'))
		self.__logFile = config.get('log', 'dir')+'/'+config.get('log', 'GlusterfsOps')
		self.__reportDir = config.get('report', 'dir')
		self.__mountDir = config.get('mount', 'dir')
		self.__glusterdDir = config.get('glusterd', 'dir')
		self.__break = Decimal(config.get('interval', 'break'))
		if taskId == "":
			taskId = "test"
		self.__taskId = taskId
		self.__report = self.__reportDir + '/' + taskId + '/report'

		self.__username = config.get('main', 'username')
		self.__password = config.get('main', 'password')

		self.__cfgLog = GlusterfsLog(self.__reportDir, self.__report, self.__logFile)
		self.__cfgLog.touchFile(self.__logFile)
		self.__cfgLog.touchFile(self.__report)
		self.__cfgLog.initProgress()
		
	def __runSshCommand(self, hostname, cmd, weight, errCode, blackList):
                '''
                execute cmd on host
		@hostname: name of host to run the command
                @cmd: command to execute
                '''
                ssh = paramiko.SSHClient ()
                ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		progress = self.__cfgLog.readProgress(self.__taskId)
		errMsg = ""
	
		#TODO: check timeout
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
			      self.__cfgLog.logProgress(progress = Decimal(progress), finished = True, code = errCode, blackList = blackList)
			      ssh.close()		
			      print >> sys.stderr, errMsg
			      return errCode
	
                self.__cfgLog.logProgress(Decimal(progress) + Decimal(weight))
                ssh.close()
		return 0

	def __runCommand(self, cmd, weight, errCode, blackList):
		self.__cfgLog.logEvent("Execution of \"%s\" start\n" % cmd)
		elapsedTime = 0
		errMsg = ""
		po = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
		while elapsedTime <= self.__cmdTimeout:
			po.poll()
			if po.returncode is None:
				time.sleep(self.__cmdInterval)
				elapsedTime = elapsedTime + self.__cmdInterval
				continue
			break

		if po.returncode is None:
			os.system("sudo kill -9 %s"%po.pid)
			errMsg = "Execution of \"%s\" failed: timeout!\n" % cmd 
		elif po.returncode != 0:
			errMsg = "Execution of \"%s\" failed: " % cmd + po.stdout.read()

		progress = self.__cfgLog.readProgress(self.__taskId)
		if errMsg != "":
                	self.__cfgLog.logError(errMsg)
			self.__cfgLog.logProgress(progress = progress, finished = True, code = errCode, blackList = blackList)
			print >> sys.stderr, errMsg
			return errCode
		else:
			self.__cfgLog.logEvent("Execution of \"%s\" end\n" % cmd)
			self.__cfgLog.logProgress(progress = Decimal(progress) + Decimal(weight))
			print po.stdout.read()
			return 0
	
	def __executePopen(self, cmd):
		self.__cfgLog.logEvent("__executePopen of \"%s\" start\n" % cmd)
		elapsedTime = 0
		errMsg = ""
		po = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
		while elapsedTime <= self.__cmdTimeout:
			po.poll()
			if po.returncode is None:
				time.sleep(self.__cmdInterval)
				elapsedTime = elapsedTime + self.__cmdInterval
				continue
			break

		if po.returncode is None:
			os.system("sudo kill -9 %s" % po.pid)
			errMsg = "__executePopen of \"%s\" failed: timeout!\n" % cmd
			self.__cfgLog.logError(errMsg)
			print >> sys.stderr, errMsg
			return None, errMsg
		elif po.returncode != 0:
			errMsg = "__executePopen of \"%s\" failed: " % cmd + po.stdout.read()
			self.__cfgLog.logError(errMsg)
			print >> sys.stderr, errMsg
		else:
			self.__cfgLog.logEvent("__executePopen of \"%s\" end\n"%cmd)

		return po.returncode, po.stdout.read()
		
	def __executeSshCommand(self, hostname, cmd):
                '''
                execute cmd on host
		@hostname: name of host to run the command
                @cmd: command to execute
                '''
		self.__cfgLog.logEvent("__executeSshCommand of \"%s\" start\n" % cmd)
                ssh = paramiko.SSHClient ()
                ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		errMsg = ""
	
		#TODO: check timeout
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
			      ssh.close()		
			      print >> sys.stderr, errMsg
			      return status
	
                ssh.close()
		self.__cfgLog.logEvent("__executeSshCommand of \"%s\" end\n" % cmd)
		return 0

	def __getUuid(self, hostname):

		if hostname == socket.gethostname():
			try:
				fp = open(self.__glusterdDir + "/glusterd.info") 
				uuid = fp.read().split("=")[1]
				uuid = uuid.rstrip()
				return uuid
			except Exception as err:
				errMsg = "Failed to get uuid from glusterd.info:%s" % str(err)
				self.__cfgLog.logError(errMsg)
                        	print >> sys.stderr, errMsg
                        	return None

		cmd = "grep " + hostname + " " + self.__glusterdDir + "/peers/*"
		#self.__cfgLog.logEvent("host ip: %s\n" % socket.gethostbyname(hostname))
		po = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
		po.wait()

		if po.returncode == 0:
			path = po.stdout.read().split(':')[0]
	                uuid = os.path.basename(path)
        	        return uuid

		if po.returncode == 1:
			cmd = "grep " + socket.gethostbyname(hostname) + " " + self.__glusterdDir + "/peers/*"
                	po = subprocess.Popen(cmd, shell = True, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
	                po.wait()
			if po.returncode == 1:
				errMsg = "Failed to find Host %s in the peer list: " % hostname
				self.__cfgLog.logError(errMsg)
       	                        print >> sys.stderr, errMsg
               	                return None

		if po.returncode != 0 and po.returncode != 1:
			errMsg = "Failed to get uuid: %s" % (po.stdout.read())
        	       	self.__cfgLog.logError(errMsg)
			print >> sys.stderr, errMsg
			return None

	def __isStarted(self, volName):
		self.__cfgLog.logEvent("__isStarted start\n")
		cmd = "sudo gluster volume info " + volName + " | grep Status"
		ret, msg = self.__executePopen(cmd)
		if ret !=0:
			self.__cfgLog.logProgress(progress = 0, finished = True, code = -4, blackList = [])
			return 1

		status = msg.split(':')[1].strip()
		if status == "Started":
			return 0

		self.__cfgLog.logEvent("__isStarted end\n")
		return 0

	def __rebuildBrickDir(self, hostname):
	  	self.__cfgLog.logEvent("__rebuildBrickDir start\n")
                cmd = "sudo gluster volume info  | grep %s" % hostname
                ret, msg= self.__executePopen(cmd)
                if ret !=0:
                        return 1

		lines = msg.strip().split('\n')
		brickDirList=[]
		for line in lines:
			brickDir = line.split(':')[2].strip()
			brickDirList.append(brickDir + ' ')
		
		brickDirNames = "".join(brickDirList)

		cmd = "sudo mkdir -p %s" % brickDirNames

		ret = self.__executeSshCommand(hostname, cmd)
		if ret !=0:
			return ret

                self.__cfgLog.logEvent("__rebuildBrickDir  end\n")
		return 0

	def triggerSelfHealing(self, volName):
		self.__cfgLog.logEvent("triggerSelfHealing start\n")
		mountPoint = self.__mountDir + "/" + volName + "/"

		ret = self.__isStarted(volName)
		if ret !=0:
			#self.__cfgLog.logProgress(progress = 0, finished = True, code = -4, blackList = [])
			return -1
		
		#Bug of GlusterFS: wait for volume to revive
                time.sleep(self.__break)

	        if os.path.isdir(mountPoint) == False:
			cmd = "mkdir -p %s" % mountPoint
			ret = self.__runCommand(cmd, weight = 0.1, errCode = -1, blackList = [])
			if ret != 0:
				return ret

		if not os.path.ismount(mountPoint):
			cmd = "sudo mount -t glusterfs " + socket.gethostname() + ":/" + volName + " " + mountPoint
			ret = self.__runCommand(cmd, weight = 0.1, errCode = -2, blackList = [])
			if ret != 0:
				return ret

		#Bug of GlusterFS: don't know when to umount
		cmd = "find %s -noleaf -print0 | xargs --null stat >/dev/null" % mountPoint
		ret = self.__runCommand(cmd, weight = 0.8, errCode = -3, blackList = [])
               	if ret != 0:
			#self.__cfgLog.logProgress(progress = 1, finished = True, code = -1, blackList = []) #modified
               		return ret
		

		self.__cfgLog.logProgress(progress = 1, finished = True, code = 0, blackList = []) #modified
		self.__cfgLog.logEvent("triggerSelfHealing end\n")
		return 0
		
	def replaceServer(self, hostname):

		self.__cfgLog.logEvent("replaceServer start\n")
		
		#TODO: write a function to Check if the input variables are correct
		#errCode = -1: failed to get uuid
		selfUuid = self.__getUuid(socket.gethostname())
		uuid = self.__getUuid(hostname)
		if uuid is None:
			self.__cfgLog.logProgress(progress = 1, finished = True, code = -1, blackList = [])
			return -1
	
		ret = self.__rebuildBrickDir(hostname)
		if ret != 0:
			return -2		

		#errCode = -3: failed to stop glusterd
		cmd = "sudo /etc/init.d/glusterfs-server stop"
		ret = self.__runSshCommand(hostname = hostname, cmd = cmd, weight = 0.2, errCode = -3, blackList = []) 
		if ret != 0:
			return ret

		#TODO: deal with the serucity issue
		#errCode = -4: failed to write uuid to glusted.info
		cmd = "sudo rm -rf %s" % self.__glusterdDir
		cmd = cmd + "; sudo mkdir -p %s" % self.__glusterdDir
		cmd = cmd + "; sudo touch %s/glusterd.info" % self.__glusterdDir
		cmd = cmd + "; sudo chmod 666 %s/glusterd.info" % self.__glusterdDir
		cmd = cmd + "; echo UUID=\"%s\">%s/glusterd.info" % (uuid, self.__glusterdDir)
		ret = self.__runSshCommand(hostname = hostname, cmd = cmd, weight = 0.2, errCode = -4, blackList = [])
		if ret != 0:
			return ret

		#errCode = -5: first restart failed
		cmd = "sudo /etc/init.d/glusterfs-server start"
                ret = self.__runSshCommand(hostname = hostname, cmd = cmd, weight = 0.2, errCode = -5, blackList = [])
                if ret != 0:
                        return ret

		time.sleep(self.__break)
		#Bug: peer probe hangs sometimes if sleep time is too short
		#errCode = -6: peer probe failed
		cmd = "sudo gluster peer probe %s" % socket.gethostname()
		ret = self.__runSshCommand(hostname = hostname, cmd = cmd, weight = 0.2, errCode = -6, blackList = [])
                if ret != 0:
                	return ret
		
		#errCode = -7: second restart failed
                cmd = "sudo /etc/init.d/glusterfs-server restart"
                ret = self.__runSshCommand(hostname = hostname, cmd = cmd, weight = 0.2, errCode = -7, blackList = [])
                if ret != 0:
                        return ret
		self.__cfgLog.logProgress(progress = 1, finished = True, code = 0, blackList = [])
		self.__cfgLog.logEvent("replaceServer end\n")
		return 0
		
	def renewReport(self):
		'''
		Clear the directroy containing the report and creae an new empty report
		'''
		self.__cfgLog.initReport()
	

if __name__ == '__main__':
	
	GO= GlusterfsOps("../DCloudGfs.ini")
	GO.renewReport()
	GO.replaceServer('ntu01')
	#GO.triggerSelfHealing("testVol")
