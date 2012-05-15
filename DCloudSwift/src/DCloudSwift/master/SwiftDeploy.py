import os, sys, socket
import posixfile
import time
import json
import subprocess
import threading
import datetime
import logging
import pickle
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser
from threading import Thread

#Self defined packages
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/"%BASEDIR)

UNNECESSARYFILES="cert* *.conf backups"
lock = threading.Lock()

from util import util
from util import threadpool
from util.SwiftCfg import SwiftCfg

class DeployProxyError(Exception):
	pass

class DeployStorageError(Exception):
	pass

class SwiftDeploy:
	def __init__(self, proxyList = [], storageList = []):
		self.__proxyList = proxyList
		self.__storageList = storageList
		self.__mentor = proxyList[0]

		self.__SC = SwiftCfg("%s/DCloudSwift/Swift.ini"%BASEDIR)
		self.__kwparams = self.__SC.getKwparams()
		self.__kwparams['proxyList'] = self.__proxyList
		self.__kwparams['storageList'] = self.__storageList
		self.__cnt = self.__kwparams['deviceCnt'] 

		self.__deployProgress = {
			'proxyProgress': 0,
			'storageProgress': 0,
			'finished': False,
			'code': 0,
			'deployedProxy': 0,
			'deployedStorage': 0,
			'blackList': [],
			'message': []
		}

		os.system("mkdir -p %s" % self.__kwparams['logDir'])

		self.__jsonStr = json.dumps(self.__kwparams)

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	def getDeployProgress(self):
		return self.__deployProgress

	def __updateProgress(self, success=False, ip="", swiftType="proxy", msg=""):
		lock.acquire()
		try:
			if swiftType == "proxy":
				self.__deployProgress['deployedProxy'] += 1
				self.__deployProgress['proxyProgress'] = (float(self.__deployProgress['deployedProxy'])/len(self.__proxyList)) * 100.0
			if swiftType == "storage":
				self.__deployProgress['deployedStorage'] += 1
				self.__deployProgress['storageProgress'] = (float(self.__deployProgress['deployedStorage'])/len(self.__storageList)) * 100.0
			if success == False:
				self.__deployProgress['blackList'].append(ip)
				self.__deployProgress['message'].append(msg)
				self.__deployProgress['code'] += 1
			if self.__deployProgress['deployedProxy'] == len(self.__proxyList) and self.__deployProgress['deployedStorage'] == len(self.__storageList):
				self.__deployProgress['finished'] = True
		finally:
			lock.release()

	def createMetadata(self):
		logger = util.getLogger(name = "createMetadata")
		proxyList = self.__kwparams['proxyList']
		storageList = self.__kwparams['storageList']
		numOfReplica = self.__kwparams['numOfReplica']
		deviceCnt = self.__kwparams['deviceCnt']
		devicePrx = self.__kwparams['devicePrx']
		versBase = int(time.time())*100000

		os.system("mkdir -p /etc/delta/swift")
		os.system("mkdir -p /etc/swift")
		os.system("touch /etc/swift/proxyList")
		os.system("touch /etc/swift/versBase")
		with open("/etc/swift/proxyList", "wb") as fh:
			pickle.dump(proxyList, fh)
			
		with open("/etc/swift/versBase", "wb") as fh:
			pickle.dump(versBase, fh)

		os.system("sh %s/DCloudSwift/proxy/CreateRings.sh %d" % (BASEDIR, numOfReplica))
		zoneNumber = 1
		for node in storageList: 
			for j in range(deviceCnt):
				deviceName = devicePrx + str(j+1)
				logger.info("DCloudSwift/proxy/AddRingDevice.sh %d %s %s"% (node["zid"], node["ip"], deviceName))
				os.system("sh %s/DCloudSwift/proxy/AddRingDevice.sh %d %s %s" % (BASEDIR,node["zid"], node["ip"], deviceName))

		os.system("sh %s/DCloudSwift/proxy/Rebalance.sh"%BASEDIR)

		os.system("cp -r /etc/swift /etc/delta")
		os.system("rm -rf /etc/swift/*")
		os.system("cd /etc/delta/swift; rm -rf %s"%UNNECESSARYFILES)

	#@timeout(self.__kwparams['proxyInterval'], -1)
	def __proxyDeploySubtask(self, proxyIP):
		logger = util.getLogger(name="proxyDeploySubtask: %s" % proxyIP)
		try:

			pathname = "/etc/delta/master/%s"%socket.gethostname()

			cmd = "ssh root@%s mkdir -p %s"%(proxyIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to mkdir /etc/delta/master for %s" % (proxyIP, stderr)
				raise DeployProxyError(errMsg)

			cmd = "ssh root@%s rm -rf %s/*"%(proxyIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to clear /etc/delta/master for %s" % (proxyIP, stderr)
				raise DeployProxyError(errMsg)

			#copy script and data to a specific directory to avoid conficts
			cmd = "scp -r %s/DCloudSwift/ root@%s:%s" %(BASEDIR, proxyIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to scp proxy deploy scripts to %s for %s" % (proxyIP, stderr)
				raise DeployProxyError(errMsg)

			cmd = "scp -r /etc/delta/swift root@%s:%s" %(proxyIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to scp metadata to %s for %s" % (proxyIP, stderr)
				raise DeployProxyError(errMsg)

			cmd = "ssh root@%s python %s/DCloudSwift/CmdReceiver.py -p %s" % (proxyIP, pathname, util.jsonStr2SshpassArg(self.__jsonStr))
			print "Start deploying proxy node %s"%proxyIP
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=500)
			if status != 0:
				errMsg = "Failed to deploy proxy %s for %s" % (proxyIP, stderr)
				raise DeployProxyError(errMsg)

			logger.info("Succeeded to deploy proxy %s" % proxyIP)
			self.__updateProgress(success=True, ip=proxyIP, swiftType="proxy", msg="")
			return 0

		except DeployProxyError as err:
			logger.error(str(err))
			self.__updateProgress(success=False, ip=proxyIP, swiftType="proxy", msg=str(err))
			return 1
		except util.TimeoutError as err:
			logger.error("%s" % str(err))
			sys.stderr.write("%s\n" % str(err))
			self.__updateProgress(success=False, ip=proxyIP, swiftType="proxy", msg=str(err))
			return 1

	def proxyDeploy(self):
		logger = util.getLogger(name="proxyDeploy")
		argumentList = []
		for i in [node["ip"] for node in self.__proxyList]:
			argumentList.append(([i], None))
		pool = threadpool.ThreadPool(10)
		requests = threadpool.makeRequests(self.__proxyDeploySubtask, argumentList)
		for req in requests:
			pool.putRequest(req)
		pool.wait()
		pool.dismissWorkers(10)
		pool.joinAllDismissedWorkers()

	#@timeout(self.__kwparams['storageInterval'], -1)
	def __storageDeploySubtask(self, storageIP):
		logger = util.getLogger(name="storageDeploySubtask: %s" % storageIP)
		try:
<<<<<<< HEAD
=======
			#if util.spreadMetadata(password=self.__kwparams['password'], sourceDir='/etc/delta/swift', nodeList=[storageIP])[0] != 0:
			#	errMsg = "Failed to spread metadata to %s" % storageIP
			#	logger.error(errMsg)
			#	self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=errMsg)
			#	return -2
>>>>>>> c7434aa7cf447eac26042a2ce6d4132f585eba22

			pathname = "/etc/delta/master/%s"%socket.gethostname()

			cmd = "ssh root@%s mkdir -p %s"%(storageIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to mkdir /etc/delta/master for %s" % (proxyIP, stderr)
				raise DeployStorageError(errMsg)

			cmd = "ssh root@%s rm -rf %s/*"%(storageIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to clear /etc/delta/master for %s" % (proxyIP, stderr)
				raise DeployStorageError(errMsg)

			cmd = "scp -r %s/DCloudSwift/ root@%s:%s" % (BASEDIR, storageIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to scp storage scripts to %s for %s" % (storageIP, stderr)
				raise DeployStorageError(errMsg)

			cmd = "scp -r /etc/delta/swift root@%s:%s" %(storageIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to scp metadata to %s for %s" % (proxyIP, stderr)
				raise DeployStorageError(errMsg)


			cmd = "ssh root@%s python %s/DCloudSwift/CmdReceiver.py -s %s"%(storageIP, pathname, util.jsonStr2SshpassArg(self.__jsonStr))
			print "Start deploying storage node %s"%storageIP
			(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
			if status != 0:
				errMsg = "Failed to deploy storage %s for %s" % (storageIP, stderr)
				raise DeployStorageError(errMsg)

			logger.info("Succeeded to deploy storage %s" % storageIP)
			self.__updateProgress(success=True, ip=storageIP, swiftType="storage", msg="")
			return 0

		except DeployStorageError as err:
			logger.error(str(err))
			self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=str(err))

		except util.TimeoutError as err:
			logger.error("%s" % err)
			sys.stderr.write("%s\n" % err)
			self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=err)

	def storageDeploy(self):
		logger = util.getLogger(name="storageDeploy")
		argumentList = []
                for i in [node["ip"] for node in self.__storageList]:
                        argumentList.append(([i], None))
                pool = threadpool.ThreadPool(20)
                requests = threadpool.makeRequests(self.__storageDeploySubtask, argumentList)
                for req in requests:
                        pool.putRequest(req)
                pool.wait()
                pool.dismissWorkers(20)
                pool.joinAllDismissedWorkers()

	def deploySwift(self):
		logger = util.getLogger(name="deploySwift")
		self.proxyDeploy()
		self.storageDeploy()

		#create a defautl account:user 
		cmd = "swauth-prep -K %s -A https://%s:8080/auth/"%(self.__kwparams['password'], self.__proxyList[0]["ip"])	
		os.system(cmd)	
		os.system("swauth-add-user -A https://%s:8080/auth -K %s -a system root testpass"% (self.__proxyList[0]["ip"], self.__kwparams['password']))

	def addStorage(self):
		logger = util.getLogger(name="addStorage")
		self.storageDeploy()

		for i in [node["ip"] for node in self.__proxyList]:
			try:
				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
				(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
				if status !=0:
					logger.error("Failed to scp proxy scrips to %s for %s"%(i, stderr))
					continue
			
				#TODO: Monitor Progress report
				cmd = "ssh root@%s python /DCloudSwift/CmdReceiver.py -a %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
				print cmd

				(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd)
			
				if status != 0:
					logger.error("Failed to addStorage from proxy %s for %s"%(i, stderr))
					continue

				#TODO: Return black list
				return (0,[],[])
			except util.TimeoutError as err:
				logger.error("%s"%err)
                        	sys.stderr.write("%s\n"%err)
		
		logger.error("Failed to addStorage\n")
		return (1, self.__proxyList, self.__storageList)

	def rmStorage(self):
		logger = util.getLogger(name="rmStorage")

		for i in [node["ip"] for node in self.__proxyList]:
			try:
				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
				(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
				if status !=0:
					logger.error("Failed to scp proxy scrips to %s for %s"%(i, stderr))
					continue
			
				#TODO: Monitor Progress report
				cmd = "ssh root@%s python /DCloudSwift/CmdReceiver.py -r %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
				print cmd

				(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd)
				if status != 0:
					logger.error("Failed to rmStorage from proxy %s for %s"%(i, stderr))
					continue

				#TODO: Return black list
				return (0,[],[])
			except util.TimeoutError as err:
				logger.error("%s"%err)
                        	sys.stderr.write("%s\n"%err)

		logger.error("Failed to rmStorage\n")
		return (1, self.__proxyList, self.__storageList)

def deploy():
	'''
	Command line implementation of swift deployment
	'''
	ret = 1

	proxyList =[]
	storageList = []
	
	try:
		proxyFile = "/etc/delta/proxyNodes"
		storageFile = "/etc/delta/storageNodes"

		#Parse proxyFile to get proxy node list
		proxyIpSet = set()
		with open(proxyFile) as fh:
			for line in fh.readlines():
				line = line.strip()
				if len(line) > 0:
					try:
						ip = line.split()[0]
						socket.inet_aton(ip)
						proxyList.append({"ip":ip})
						if ip in proxyIpSet:
							raise Exception("%s contains duplicate ip"%proxyFile)
						proxyIpSet.add(ip)

					except socket.error:
						raise Exception("%s contains a invalid ip %s"%(proxyFile, ip))

		#Parse storageFile to get storage node list
		storageIpSet = set()
		with open(storageFile) as fh:
			for line in fh.readlines():
				line = line.strip()
				if len(line) > 0:
					tokens = line.split()
					if len(tokens) != 2:
						raise Exception("%s contains a invalid line %s"%(storageFile, line))
					try:
						ip=tokens[0]
						zid = int(tokens[1])
						socket.inet_aton(ip)

						if zid < 1 or zid > 9:
							raise Exception("zid has to be a positive integer < 10")
						storageList.append({"ip": ip, "zid": zid})
						if ip in storageIpSet:
							raise Exception("%s contains duplicate ip"%storageFile)

						storageIpSet.add(ip)
					except socket.error:
						raise Exception("%s contains a invalid ip %s"%(storageFile, ip))
					except ValueError:
						raise Exception("%s contains a invalid zid %s"%(storageFile, tokens[1]))
						

		SD = SwiftDeploy(proxyList, storageList)
		SD.createMetadata()
		t = Thread(target=SD.deploySwift, args=())
		t.start()
		progress = SD.getDeployProgress()
		while progress['finished'] != True:
			time.sleep(5)
			print progress
			progress = SD.getDeployProgress()
		print "Swift deploy process is done!"

		return 0
	except IOError as e:
		print >>sys.stderr,  "Failed to access input files for %s"%str(e)
	except Exception as e:
		print >>sys.stderr, str(e)
	finally:
		return ret

if __name__ == '__main__':
	pass
	#util.spreadPackages(password="deltacloud", nodeList=["172.16.229.122", "172.16.229.34", "172.16.229.46", "172.16.229.73"])
	#util.spreadRC(password="deltacloud", nodeList=["172.16.229.122"])
	#SD = SwiftDeploy([{"ip":"172.16.229.82"}], [{"ip":"172.16.229.145", "zid":1}])
	#SD = SwiftDeploy([{"ip":"172.16.229.35"}], [{"ip":"172.16.229.146", "zid":1}, {"ip":"172.16.229.35", "zid":2}])
	
	#SD.createMetadata()
	#t = Thread(target=SD.deploySwift, args=())
	#t.start()
	#progress = SD.getDeployProgress()
	#while progress['finished'] != True:
	#	time.sleep(5)
	#	print progress
	#	progress = SD.getDeployProgress()
	#print "Swift deploy process is done!"
	#SD.deploySwift()
