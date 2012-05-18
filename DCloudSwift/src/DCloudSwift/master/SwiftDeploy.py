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


class UpdateMetadataError(Exception):
	pass


class SpreadRingFilesError(Exception):
	pass

class SwiftDeploy:
	def __init__(self):
		self.__deltaDir = "/etc/delta"
		self.__SC = SwiftCfg("%s/DCloudSwift/Swift.ini"%BASEDIR)
		self.__kwparams = self.__SC.getKwparams()

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

		self.__spreadProgress = {
			'progress': 0,
			'updatedNodes':0,
			'finished': False,
			'code': 0,
			'blackList': [],
			'message': []
		}
		os.system("mkdir -p %s" % self.__kwparams['logDir'])

		self.__jsonStr = json.dumps(self.__kwparams)

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	def getDeployProgress(self):
		return self.__deployProgress

	def getSpreadProgress(self):
		return self.__spreadProgress

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

	def __updateSpreadProgress(self, success=False, ip="", msg=""):
		lock.acquire()
		try:
			numOfTasks = len(self.__nodes2Update)

			self.__spreadProgress['updatedNodes'] += 1
			self.__spreadProgress['progress'] = (float(self.__spreadProgress['updatedNodes'])/numOfTasks) * 100.0

			if success == False:
				self.__spreadProgress['blackList'].append(ip)
				self.__spreadProgress['message'].append(msg)
				self.__spreadProgress['code'] += 1

			if self.__spreadProgress['updatedNodes'] == numOfTasks:
				self.__spreadProgress['finished'] = True
		finally:
			lock.release()

	def __createMetadata(self, proxyList, storageList):
		logger = util.getLogger(name = "createMetadata")
		numOfReplica = self.__kwparams['numOfReplica']
		deviceCnt = self.__kwparams['deviceCnt']
		devicePrx = self.__kwparams['devicePrx']
		versBase = int(time.time())*100000
		swiftDir = "%s/swift"%self.__deltaDir

		if os.path.isdir(swiftDir):
			os.system("rm -rf %s"%swiftDir)

		os.system("mkdir -p %s"%swiftDir)
		os.system("touch %s/proxyList"%swiftDir)
		os.system("touch %s/versBase"%swiftDir)
		with open("%s/proxyList"%swiftDir, "wb") as fh:
			pickle.dump(proxyList, fh)
			
		with open("%s/versBase"%swiftDir, "wb") as fh:
			pickle.dump(versBase, fh)

		os.system("sh %s/DCloudSwift/proxy/CreateRings.sh %d %s" % (BASEDIR, numOfReplica, swiftDir))
		for node in storageList: 
			for j in range(deviceCnt):
				deviceName = devicePrx + str(j+1)
				cmd = "sh %s/DCloudSwift/proxy/AddRingDevice.sh %d %s %s %s"% (BASEDIR, node["zid"], node["ip"], deviceName, swiftDir)
				logger.info(cmd)
				os.system(cmd)

		os.system("sh %s/DCloudSwift/proxy/Rebalance.sh %s"%(BASEDIR,swiftDir))
		os.system("cd %s; rm -rf %s"%(swiftDir,UNNECESSARYFILES))

	def __updateMetadata2AddNodes(self, proxyList, storageList):
		logger = util.getLogger(name = "__updateMetadata2AddNodes")
		numOfReplica = self.__kwparams['numOfReplica']
		deviceCnt = self.__kwparams['deviceCnt']
		devicePrx = self.__kwparams['devicePrx']

		swiftDir = "%s/swift"%self.__deltaDir
		
		oriSwiftNodeIpSet = set(util.getSwiftNodeIpList(swiftDir))
		oriProxyList = []
		with open("%s/proxyList"%swiftDir, "rb") as fh:
			oriProxyList=pickle.load(fh)

		for node in proxyList:
			if node["ip"] in oriSwiftNodeIpSet:
				raise UpdateMetadataError("Node %s already exists"%node["ip"])
			
		completeProxyList = oriProxyList + proxyList

		for node in storageList:
			if node["ip"] in oriSwiftNodeIpSet:
				raise UpdateMetadataError("Node %s already exists"%node["ip"])
			
		with open("%s/proxyList"%swiftDir, "wb") as fh:
			pickle.dump(completeProxyList, fh)

		for node in storageList: 
			for j in range(deviceCnt):
				deviceName = devicePrx + str(j+1)
				cmd = "sh %s/DCloudSwift/proxy/AddRingDevice.sh %d %s %s %s"% (BASEDIR, node["zid"], node["ip"], deviceName, swiftDir)
				logger.info(cmd)
				os.system(cmd)

		os.system("sh %s/DCloudSwift/proxy/Rebalance.sh %s"%(BASEDIR, swiftDir))
		os.system("cd %s; rm -rf %s"%(swiftDir,UNNECESSARYFILES))

	def __proxyDeploySubtask(self, proxyIP):
		logger = util.getLogger(name="proxyDeploySubtask: %s" % proxyIP)
		try:

			print "Start deploying proxy node %s"%proxyIP
			pathname = "/etc/delta/master/%s"%socket.gethostname()

			cmd = "ssh root@%s mkdir -p %s"%(proxyIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to mkdir root@%s:/etc/delta/master for %s" % (proxyIP, stderr)
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
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=500)
			if status != 0:
				errMsg = "Failed to deploy proxy %s for %s" % (proxyIP, stderr)
				raise DeployProxyError(errMsg)

			logger.info("Succeeded to deploy proxy %s" % proxyIP)
			self.__updateProgress(success=True, ip=proxyIP, swiftType="proxy", msg="")

		except DeployProxyError as err:
			logger.error(str(err))
			self.__updateProgress(success=False, ip=proxyIP, swiftType="proxy", msg=str(err))
		except util.TimeoutError as err:
			logger.error("%s" % str(err))
			self.__updateProgress(success=False, ip=proxyIP, swiftType="proxy", msg=str(err))
		except Exception as err:
			msg = "Failed to deploy %s for %s"%(proxyIP,str(err))
			logger.error(msg)
			self.__updateProgress(success=False, ip=proxyIP, swiftType="proxy", msg=msg)

	def __proxyDeploy(self):
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

	def __storageDeploySubtask(self, storageIP):
		logger = util.getLogger(name="storageDeploySubtask: %s" % storageIP)
		try:
			pathname = "/etc/delta/master/%s"%socket.gethostname()

			print "Start deploying storage node %s"%storageIP
			cmd = "ssh root@%s mkdir -p %s"%(storageIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to mkdir root@%s:/etc/delta/master for %s" % (storageIP, stderr)
				raise DeployStorageError(errMsg)

			cmd = "ssh root@%s rm -rf %s/*"%(storageIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to clear /etc/delta/master for %s" % (storageIP, stderr)
				raise DeployStorageError(errMsg)

			cmd = "scp -r %s/DCloudSwift/ root@%s:%s" % (BASEDIR, storageIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to scp storage scripts to %s for %s" % (storageIP, stderr)
				raise DeployStorageError(errMsg)

			cmd = "scp -r /etc/delta/swift root@%s:%s" %(storageIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to scp metadata to %s for %s" % (storageIP, stderr)
				raise DeployStorageError(errMsg)


			cmd = "ssh root@%s python %s/DCloudSwift/CmdReceiver.py -s %s"%(storageIP, pathname, util.jsonStr2SshpassArg(self.__jsonStr))
			(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
			if status != 0:
				errMsg = "Failed to deploy storage %s for %s" % (storageIP, stderr)
				raise DeployStorageError(errMsg)

			logger.info("Succeeded to deploy storage %s" % storageIP)
			self.__updateProgress(success=True, ip=storageIP, swiftType="storage", msg="")

		except DeployStorageError as err:
			logger.error(str(err))
			self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=str(err))

		except util.TimeoutError as err:
			logger.error("%s" % err)
			self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=err)
		except Exception as err:
			msg = "Failed to deploy %s for %s"%(storageIP,str(err))
			logger.error(msg)
			self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=msg)

	def __storageDeploy(self):
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

	def deploySwift(self, proxyList, storageList):
		logger = util.getLogger(name="deploySwift")

		self.__createMetadata(proxyList, storageList)
		self.__proxyList = proxyList
		self.__storageList = storageList
		self.__proxyDeploy()
		self.__storageDeploy()

		#create a defautl account:user 
		cmd = "swauth-prep -K %s -A https://%s:8080/auth/"%(self.__kwparams['password'], self.__proxyList[0]["ip"])	
		os.system(cmd)	
		os.system("swauth-add-user -A https://%s:8080/auth -K %s -a system root testpass"% (self.__proxyList[0]["ip"], self.__kwparams['password']))

	def __spreadRingFilesSubtask(self, nodeIP):
		logger = util.getLogger(name="spreadRingFilesSubtask: %s" % nodeIP)
		try:
			pathname = "/etc/delta/master/%s"%socket.gethostname()

			print "Start updating ring files on node %s"%nodeIP
			cmd = "ssh root@%s mkdir -p %s"%(nodeIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to mkdir root@%s:/etc/delta/master for %s" % (nodeIP, stderr)
				raise SpreadRingFilesError(errMsg)

			cmd = "ssh root@%s rm -rf %s/*"%(nodeIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to clear /etc/delta/master for %s" % (nodeIP, stderr)
				raise SpreadRingFilesError(errMsg)

			cmd = "scp -r /etc/delta/swift root@%s:%s" %(nodeIP, pathname)
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to scp ring files to %s for %s" % (nodeIP, stderr)
				raise SpreadRingFilesError(errMsg)


			cmd = "ssh root@%s python /DCloudSwift/CmdReceiver.py -u %s/swift"%(nodeIP, pathname)
			(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
			if status != 0:
				errMsg = "Failed to update ring files on %s for %s" % (nodeIP, stderr)
				raise SpreadRingFilesError(errMsg)

			self.__updateSpreadProgress(success=True, ip=nodeIP, msg="")
			logger.info("Succeeded to update ring files on %s" % nodeIP)

		except SpreadRingFilesError as err:
			logger.error("%s"%str(err))
			self.__updateSpreadProgress(success=False, ip=nodeIP, msg=str(err))
		except util.TimeoutError as err:
			msg="Failed to update ring files on %s for %s"%(nodeIP, std(err))
			logger.error("%s" % err)
			self.__updateSpreadProgress(success=False, ip=nodeIP, msg=msg)
		except Exception as err:
			msg = "Failed to update ring files on %s for %s"%(nodeIP,str(err))
			logger.error(msg)
			self.__updateSpreadProgress(success=False, ip=nodeIP, msg=msg)

	def spreadRingFiles(self):
		swiftDir = "%s/swift"%self.__deltaDir
		swiftNodeIpList = util.getSwiftNodeIpList(swiftDir)

		self.__nodes2Update = swiftNodeIpList

		argumentList = []
                for i in swiftNodeIpList:
                        argumentList.append(([i], None))
                pool = threadpool.ThreadPool(20)
                requests = threadpool.makeRequests(self.__spreadRingFilesSubtask, argumentList)
                for req in requests:
                        pool.putRequest(req)
                pool.wait()
                pool.dismissWorkers(20)
                pool.joinAllDismissedWorkers()

	def addNodes(self, proxyList=[], storageList=[]):
		logger = util.getLogger(name="addStorage")
		self.__updateMetadata2AddNodes(proxyList=proxyList, storageList=storageList)
		self.__proxyList = proxyList
		self.__storageList = storageList
		self.__proxyDeploy()
		self.__storageDeploy()
		self.spreadRingFiles()
		
	def rmNodes(self):
		logger = util.getLogger(name="rmStorage")


def parseNodeFiles(proxyFile, storageFile):
	proxyList =[]
	storageList = []

	try:
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
		return (proxyList, storageList)
	except IOError as e:
		msg = "Failed to access input files for %s"%str(e)
		raise Exception(msg)
		
		

def addNodes():
	'''
	Command line implementation of swift deployment
	'''
	ret = 1

	proxyFile = "/etc/delta/addProxyNodes"
	storageFile = "/etc/delta/addStorageNodes"
	try:
						
		(proxyList, storageList) = parseNodeFiles(proxyFile=proxyFile, storageFile=storageFile)
		print proxyList, storageList

		SD = SwiftDeploy()
		t = Thread(target=SD.addNodes, args=(proxyList, storageList))
		t.start()
		progress = SD.getDeployProgress()
		while progress['finished'] != True:
			time.sleep(10)
			print progress
			progress = SD.getDeployProgress()
		print "Swift deploy is done!"

		print "Start to spread ring files"

		spreadProgress = SD.getSpreadProgress()
		while spreadProgress['finished'] != True:
			time.sleep(8)
			print  spreadProgress
			spreadProgress = SD.getSpreadProgress()


		print "Finished to spread ring files"
		return 0
	except Exception as e:
		print >>sys.stderr, str(e)
	finally:
		return ret

def deploy():
	'''
	Command line implementation of swift deployment
	'''

	ret = 1

	proxyFile = "/etc/delta/proxyNodes"
	storageFile = "/etc/delta/storageNodes"
	try:
						
		(proxyList, storageList) = parseNodeFiles(proxyFile=proxyFile, storageFile=storageFile)

		SD = SwiftDeploy()
		t = Thread(target=SD.deploySwift, args=(proxyList, storageList))
		t.start()
		progress = SD.getDeployProgress()
		while progress['finished'] != True:
			time.sleep(8)
			print progress
			progress = SD.getDeployProgress()
		print "Swift deploy process is done!"

		return 0
	except Exception as e:
		print >>sys.stderr, str(e)
	finally:
		return ret

if __name__ == '__main__':
	#SD = SwiftDeploy()
	#t = Thread(target=SD.spreadRingFiles, args=())
	#t.start()
	#progress = SD.getSpreadProgress()
	#while progress['finished'] != True:
	#	time.sleep(10)
	#	print progress
	#	progress = SD.getSpreadProgress()

	#spreadProgress = SD.getSpreadProgress()
	pass
	#util.spreadPackages(password="deltacloud", nodeList=["172.16.229.122", "172.16.229.34", "172.16.229.46", "172.16.229.73"])
	#util.spreadRC(password="deltacloud", nodeList=["172.16.229.122"])
	#SD = SwiftDeploy([{"ip":"172.16.229.82"}], [{"ip":"172.16.229.145", "zid":1}])
	#SD = SwiftDeploy([{"ip":"172.16.229.35"}], [{"ip":"172.16.229.146", "zid":1}, {"ip":"172.16.229.35", "zid":2}])
	
