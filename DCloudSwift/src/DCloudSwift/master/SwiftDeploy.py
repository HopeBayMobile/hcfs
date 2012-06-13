import os, sys, socket
import posixfile
import time
import json
import subprocess
import threading
import datetime
import logging
import pickle
import collections
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser
from threading import Thread

#Self defined packages
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/"%BASEDIR)

from util import util
from util import threadpool
from util.SwiftCfg import SwiftCfg
from util.util import GlobalVar

UNNECESSARYFILES="cert* *.conf backups"
lock = threading.Lock()

class CleanNodeError(Exception):
	pass

class DeployProxyError(Exception):
	pass

class DeployStorageError(Exception):
	pass

class DeploySwiftError(Exception):
	pass

class UpdateMetadataError(Exception):
	pass

class SpreadRingFilesError(Exception):
	pass

class SwiftDeploy:
	def __init__(self, conf=GlobalVar.ORI_SWIFTCONF):
		logger = util.getLogger(name="SwiftDeploy.__init__")

		if os.path.isfile(conf):
			cmd = "cp %s %s"%(conf, GlobalVar.SWIFTCONF)
			os.system(cmd)
		else:
			msg = "Confing %s does not exist"%conf
			print >> sys.stderr, msg
			logger.warn(msg)

		if not os.path.isfile(GlobalVar.SWIFTCONF):
			msg ="Config %s does not exist"%GlobalVar.SWIFTCONF
			print >> sys.stderr, msg
			logger.error(msg)
			sys.exit(1)


		self.__deltaDir = GlobalVar.DELTADIR
		os.system("mkdir -p %s"%self.__deltaDir)

		self.__SC = SwiftCfg(GlobalVar.SWIFTCONF)
		self.__kwparams = self.__SC.getKwparams()
		self.__jsonStr = json.dumps(self.__kwparams)
		os.system("mkdir -p %s" % self.__kwparams['logDir'])

		self.__setUpdateMetadataProgress()
		self.__setDeployProgress()
		self.__setSpreadProgress() 
		self.__setCleanProgress()

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")


	def getUpdateMetadataProgress(self):
		return self.__updateMetadataProgress

	def getDeployProgress(self):
		return self.__deployProgress

	def getSpreadProgress(self):
		return self.__spreadProgress

	def getCleanProgress(self):
		return self.__cleanProgress

	def __setUpdateMetadataProgress(self, progress=0, finished=False, message=[], code=0):
		lock.acquire()
		try:
			self.__updateMetadataProgress = {
				'progress': progress,
				'finished': finished,
				'code': code,
				'message': message
			}

		finally:
			lock.release()

	def __setDeployProgress(self, proxyProgress=0, storageProgress=0, finished=False, message=[], code=0, deployedProxy=0, deployedStorage=0, blackList=[]):
		lock.acquire()
		try:
			self.__deployProgress = {
				'proxyProgress': proxyProgress,
				'storageProgress': storageProgress,
				'finished': finished,
				'code': code,
				'deployedProxy': deployedProxy,
				'deployedStorage': deployedStorage,
				'blackList': blackList,
				'message': message
			}

		finally:
			lock.release()

	def __setSpreadProgress(self, progress=0, doneTasks=0, finished=False, code=0, blackList=[], message=[]):
		lock.acquire()
		try:

			self.__spreadProgress = {
				'progress': progress,
				'doneTasks': doneTasks,
				'finished': finished,
				'code': code,
				'blackList': blackList,
				'message': message
			}

		finally:
			lock.release()

	def __setCleanProgress(self, progress=0, doneTasks=0, finished=False, code=0, blackList=[], message=[]):
		lock.acquire()
		try:

			self.__cleanProgress = {
				'progress': progress,
				'doneTasks': doneTasks,
				'finished': finished,
				'code': code,
				'blackList': blackList,
				'message': message
			}

		finally:
			lock.release()

	def __updateDeployProgress(self, success=False, ip="", swiftType="proxy", msg=""):
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
			if self.__deployProgress['deployedProxy'] == len(self.__proxyList) and self.__deployProgress['deployedStorage'] == len(self.__storageList):
				
				swiftDir = self.__deltaDir+'/swift'
				numOfReplica = util.getNumOfReplica(swiftDir)

				if numOfReplica is None:
					self.__deployProgress['code'] = 1
                                        self.__deployProgress['message'].append("Failed to get numOfReplica")
					
				else:
					ret = self.__isDeploymentOk(self.__proxyList, self.__storageList, self.__deployProgress['blackList'], numOfReplica)
					if ret.val == False:
						self.__deployProgress['code'] = 1
						self.__deployProgress['message'].append(ret.msg)
					else:
						self.__deployProgress['code'] = 0

				self.__deployProgress['finished'] = True
		finally:
			lock.release()

	def __updateSpreadProgress(self, success=False, ip="", msg=""):
		lock.acquire()
		try:
			numOfTasks = len(self.__nodes2Process)

			self.__spreadProgress['doneTasks'] += 1
			self.__spreadProgress['progress'] = (float(self.__spreadProgress['doneTasks'])/numOfTasks) * 100.0
			

			if success == False:
				self.__spreadProgress['blackList'].append(ip)
				self.__spreadProgress['message'].append(msg)
				self.__spreadProgress['code'] += 1

			if self.__spreadProgress['doneTasks'] == numOfTasks:
				self.__spreadProgress['finished'] = True
		finally:
			lock.release()

	def __updateCleanProgress(self, success=False, ip="", msg=""):
		lock.acquire()
		try:
			numOfTasks = len(self.__nodes2Process)

			self.__cleanProgress['doneTasks'] += 1
			self.__cleanProgress['progress'] = (float(self.__cleanProgress['doneTasks'])/numOfTasks) * 100.0
			

			if success == False:
				self.__cleanProgress['blackList'].append(ip)
				self.__cleanProgress['message'].append(msg)
				self.__cleanProgress['code'] += 1

			if self.__cleanProgress['doneTasks'] == numOfTasks:
				self.__cleanProgress['finished'] = True
		finally:
			lock.release()

	def __createMetadata(self, proxyList, storageList, numOfReplica):
		logger = util.getLogger(name = "createMetadata")
		
		try:
			self.__setUpdateMetadataProgress()
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
			self.__setUpdateMetadataProgress(progress=100, code=0, finished=True)
		except Exception as e:
			logger.error(str(e))
			self.__setUpdateMetadataProgress(finished=True, code=1, message=[str(e)])
			raise UpdateMetadataError(str(e))

	def __updateMetadata2AddNodes(self, proxyList, storageList):
		logger = util.getLogger(name = "__updateMetadata2AddNodes")
		try:
			self.__setUpdateMetadataProgress()

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
			self.__setUpdateMetadataProgress(code=0, finished=True, progress=100)

		except Exception as e:
			self.__setUpdateMetadataProgress(finished=True, code=1, message=[str(e)])
			raise UpdateMetadataError(str(e))


	def __updateMetadata2DeleteNodes(self, proxyList, storageList):
		logger = util.getLogger(name = "__updateMetadata2DeleteNodes")
		try:
			self.__setUpdateMetadataProgress()
			deviceCnt = self.__kwparams['deviceCnt']
			devicePrx = self.__kwparams['devicePrx']

			swiftDir = "%s/swift"%self.__deltaDir

			oriSwiftNodeIpSet = set(util.getSwiftNodeIpList(swiftDir))
			oriProxyList = []
			with open("%s/proxyList"%swiftDir, "rb") as fh:
				oriProxyList=pickle.load(fh)

			for node in proxyList:
				if not node["ip"] in oriSwiftNodeIpSet:
					raise UpdateMetadataError("Node %s does not exist!"%node["ip"])
				

			completeProxyList =[node for node in oriProxyList if node not in proxyList]

			for node in storageList:
				if not node["ip"] in oriSwiftNodeIpSet:
					raise UpdateMetadataError("Node %s does not exist!"%node["ip"])
			

			deletedIpList = [node["ip"] for node in storageList] + [node["ip"] for node in proxyList]
			safe = self.isDeletionOfNodesSafe(deletedIpList, swiftDir)
			if safe.val == False:
				raise UpdateMetadataError("Unsafe to delete nodes for %s"%safe.msg)

			with open("%s/proxyList"%swiftDir, "wb") as fh:
				pickle.dump(completeProxyList, fh)

			for node in storageList: 
				for j in range(deviceCnt):
					deviceName = devicePrx + str(j+1)
					cmd = "sh %s/DCloudSwift/proxy/DeleteRingDevice.sh %s %s %s"% (BASEDIR, node["ip"], deviceName, swiftDir)
					logger.info(cmd)
					os.system(cmd)

			os.system("sh %s/DCloudSwift/proxy/Rebalance.sh %s"%(BASEDIR, swiftDir))
			os.system("cd %s; rm -rf %s"%(swiftDir,UNNECESSARYFILES))

			self.__setUpdateMetadataProgress(finished=True, code=0)
		except Exception as e:
			self.__setUpdateMetadataProgress(finished=True, code=1, message=[str(e)])
			raise UpdateMetadataError(str(e))

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
			self.__updateDeployProgress(success=True, ip=proxyIP, swiftType="proxy", msg="")

		except DeployProxyError as err:
			logger.error(str(err))
			self.__updateDeployProgress(success=False, ip=proxyIP, swiftType="proxy", msg=str(err))
		except util.TimeoutError as err:
			logger.error("%s" % str(err))
			self.__updateDeployProgress(success=False, ip=proxyIP, swiftType="proxy", msg=str(err))
		except Exception as err:
			msg = "Failed to deploy %s for %s"%(proxyIP,str(err))
			logger.error(msg)
			self.__updateDeployProgress(success=False, ip=proxyIP, swiftType="proxy", msg=msg)

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
			self.__updateDeployProgress(success=True, ip=storageIP, swiftType="storage", msg="")

		except DeployStorageError as err:
			logger.error(str(err))
			self.__updateDeployProgress(success=False, ip=storageIP, swiftType="storage", msg=str(err))

		except util.TimeoutError as err:
			logger.error("%s" % err)
			self.__updateDeployProgress(success=False, ip=storageIP, swiftType="storage", msg=err)
		except Exception as err:
			msg = "Failed to deploy %s for %s"%(storageIP,str(err))
			logger.error(msg)
			self.__updateDeployProgress(success=False, ip=storageIP, swiftType="storage", msg=msg)

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

	def __deploySwift(self, proxyList, storageList):
		logger = util.getLogger(name="__deploySwift")
		try: 
			self.__setDeployProgress()
			self.__proxyList = proxyList
			self.__storageList = storageList
			self.__proxyDeploy()
			self.__storageDeploy()
		except Exception as e:
			logger.error(str(e))
			self.__setDeployProgress(finished=True, code=1, message=[str(e)])
			raise DeploySwiftError(str(e))
	
	def deploySwift(self, proxyList, storageList, numOfReplica):
		logger = util.getLogger(name="deploySwift")

		try:
			os.system("rm %s"%GlobalVar.ACCOUNT_DB)
			self.__createMetadata(proxyList, storageList, numOfReplica)
			self.__deploySwift(proxyList, storageList)
		except UpdateMetadataError, DeploySwiftError: 
			return

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

	def __cleanNodesSubtask(self, nodeIp):
		'''
		Stop swift services and clear related metadata from memory and disks.
		'''
		logger = util.getLogger(name="cleanNodeSubtask: %s" % nodeIp)
		try:

			cmd = "ssh root@%s python /DCloudSwift/CmdReceiver.py -c %s"%(nodeIp, util.jsonStr2SshpassArg("{}"))
			(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
			if status != 0:
				errMsg = "Failed to clean %s for %s" % (nodeIp, stderr)
				raise CleanNodeError(errMsg)

			logger.info("Succeeded to clean node %s" % nodeIp)
			self.__updateCleanProgress(success=True, ip=nodeIp, msg="")

		except CleanNodeError as err:
			logger.error(str(err))
			self.__updateCleanProgress(success=False, ip=nodeIp, msg=str(err))

		except util.TimeoutError as err:
			logger.error("%s" % err)
			self.__updateCleanProgress(success=False, ip=nodeIp, msg=err)

		except Exception as err:
			msg = "Failed to clean %s for %s"%(nodeIp, str(err))
			logger.error(msg)
			self.__updateCleanProgress(success=False, ip=nodeIp, msg=msg)

	def isDeletionOfNodesSafe(self,ipList, swiftDir=None):
		zidSet = set()
		ip2Zid ={}
		val = False 
		msg = ""

		if swiftDir is None:
			swiftDir = "%s/swift"%self.__deltaDir

		try:

			ip2Zid = util.getIp2Zid(swiftDir)
			if ip2Zid is None:
				raise Exception("Failed to get ip to zid mapping from %s/object.builder"%swiftDir)

			numOfReplica = util.getNumOfReplica(swiftDir)
			if numOfReplica is None:
                                raise Exception("Failed to get number of replica from %s/object.builder"%swiftDir)

			#Criteria: the remaining number of zones has to >= the number of replica
			for ip in ipList:
				ip2Zid[ip] = None

			for ip in ip2Zid:
				if ip2Zid[ip] is not None:
					zidSet.add(ip)

			if len(zidSet) >= numOfReplica:
				val = True
			else:
				msg = "Number of zones has to be greater than or equal to number of replica"
			
		except Exception as e:
			val = False
			msg = str(e)
		finally:
			Bool = collections.namedtuple("Bool", "val msg")
			return Bool(val, msg)

	def __isDeploymentOk(self, proxyList, storageList, blackList, numOfReplica):
		zidSet = set()
		failedZones = set()
		proxyIpSet = set()
		failedProxyIpSet = set()
		ip2Zid ={}
		val = False 
		msg = ""

		try:
			#Criteria1: number of zones has to be greater than or equal to number of replica
			for node in storageList:
				zidSet.add(node["zid"])

			if len(zidSet) < numOfReplica:
				raise Exception("Number of zones is less than number of replica.")		


			#Criteria2: number of zones containing failed nodes has to be less than number of replica
			for node in storageList:
				ip2Zid.setdefault(node["ip"], node["zid"])

			for ip in blackList:
				if ip2Zid.get(ip) is not None:
					failedZones.add(ip2Zid[ip])

			if len(failedZones) >= numOfReplica:
				raise Exception("More than %d (number of replica) zones containing failed nodes"%numOfReplica) 

			#Criteria3: at least one proxy node is healthy
			for node in proxyList:
				proxyIpSet.add(node["ip"])
		
			for ip in blackList:
				if ip in proxyIpSet:
					failedProxyIpSet.add(ip)

			if len(proxyIpSet) == len(failedProxyIpSet):
				raise Exception("No proxy node is successfully deployed")

			val = True

		except Exception as e:
			val = False
			msg = str(e)
		finally:
			Bool = collections.namedtuple("Bool", "val msg")
			return Bool(val, msg)


	def spreadRingFiles(self):
		swiftDir = "%s/swift"%self.__deltaDir
		swiftNodeIpList = util.getSwiftNodeIpList(swiftDir)

		self.__nodes2Process = swiftNodeIpList

		self.__setSpreadProgress()

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
		try:
			self.__updateMetadata2AddNodes(proxyList=proxyList, storageList=storageList)
		except UpdateMetadataError as e:
			logger.error(str(e))
			return

		try:
			self.__setDeployProgress()
			self.__proxyList = proxyList
			self.__storageList = storageList
			self.__proxyDeploy()
			self.__storageDeploy()
		except Exception as e:
			logger.error(str(e))
			self.__setDeployProgress(finished=True, code=1, message=[str(e)])


		try:
			self.spreadRingFiles()
		except Exception as e:
			logger.error(str(e))
			self.__setSpreadProgress(finished=True, code=1, message=[str(e)])
			
		
	def cleanNodes(self, ipList):
		logger = util.getLogger(name="cleanNodes")

		self.__setCleanProgress()
		self.__nodes2Process = ipList
		argumentList = []
                for i in ipList:
                        argumentList.append(([i], None))

                pool = threadpool.ThreadPool(20)
                requests = threadpool.makeRequests(self.__cleanNodesSubtask, argumentList)
                for req in requests:
                        pool.putRequest(req)
                pool.wait()
                pool.dismissWorkers(20)
                pool.joinAllDismissedWorkers()

	def deleteNodes(self, proxyList=[], storageList=[]):
		logger = util.getLogger(name="deleteStorage")

		try:
			self.__updateMetadata2DeleteNodes(proxyList=proxyList, storageList=storageList)
		except UpdateMetadataError as e:
			logger.error(str(e))
			return

		try:
			self.__setSpreadProgress()
			self.spreadRingFiles()
		except Exception as e:
			logger.error(str(e))
			self.__setSpreadProgress(finished=True, code=1, message=[str(e)])

		try:
			
			deletedProxyIpSet = set([node["ip"] for node in proxyList])
			deletedStorageIpSet = set([node["ip"] for node in storageList])
			deletedNodeIpList = list(deletedProxyIpSet | deletedStorageIpSet)
			self.cleanNodes(deletedNodeIpList)

		except Exception as e:
			logger.error(str(e))
			self.__setCleanProgress(finished=True, code=1, message=[str(e)])

		

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
		

def getSection(inputFile, section):
	ret = []
	with open(inputFile) as fh:
		lines = fh.readlines()
		start = 0
		for i in range(len(lines)):
			line = lines[i].strip()
			if line.startswith('[') and section in line:
				start = i+1
				break
		end = len(lines)
		for i in range(start, len(lines)):
			line = lines[i].strip()
			if line.startswith('['):
				end = i
				break


		for line in lines[start:end]:
			line = line.strip()
			if len(line) > 0:
				ret.append(line)

		return ret
		
def parseAddNodesSection(inputFile):
	lines = getSection(inputFile,"addNodes")
	try:
		proxyList = []
		storageList = []
		ipSet = set()
		for line in lines:
			line = line.strip()
			if len(line) > 0:
				tokens = line.split()
				if len(tokens) != 2:
					raise Exception("[addNodes] contains an invalid line %s"%line)
				try:
					ip=tokens[0]
					zid = int(tokens[1])
					socket.inet_aton(ip)

					if zid < 1 or zid > 9:
						raise Exception("zid has to be a positive integer < 10")

					if ip in ipSet:
						raise Exception("[addNodes] contains duplicate ip")

					proxyList.append({"ip":ip})
					storageList.append({"ip": ip, "zid": zid})
					ipSet.add(ip)
				except socket.error:
					raise Exception("[addNodes] contains an invalid ip %s"%ip)
				except ValueError:
					raise Exception("[addNodes] contains an invalid zid %s"%tokens[1])
		return (proxyList, storageList)
	except IOError as e:
		msg = "Failed to access input files for %s"%str(e)
		raise Exception(msg)

def parseDeleteNodesSection(inputFile):
	lines = getSection(inputFile,"deleteNodes")
	try:
		proxyList = []
		storageList = []
		ipSet = set()
		for line in lines:
			line = line.strip()
			if len(line) > 0:
				tokens = line.split()
				try:
					ip=tokens[0]
					socket.inet_aton(ip)

					if ip in ipSet:
						raise Exception("[deleteNodes] contains duplicate ip")

					proxyList.append({"ip":ip})
					storageList.append({"ip": ip})
					ipSet.add(ip)
				except socket.error:
					raise Exception("[deleteNodes] contains an invalid ip %s"%ip)

		return (proxyList, storageList)
	except IOError as e:
		msg = "Failed to access input files for %s"%str(e)
		raise Exception(msg)

def parseDeploySection(inputFile):
	lines = getSection(inputFile,"deploy")
	try:
		proxyList = []
		storageList = []
		ipSet = set()
		for line in lines:
			line = line.strip()
			if len(line) > 0:
				tokens = line.split()
				if len(tokens) != 2:
					raise Exception("[deploy] contains an invalid line %s"%line)
				try:
					ip=tokens[0]
					zid = int(tokens[1])
					socket.inet_aton(ip)

					if zid < 1 or zid > 9:
						raise Exception("zid has to be a positive integer < 10")

					if ip in ipSet:
						raise Exception("[deploy] contains duplicate ip")

					proxyList.append({"ip":ip})
					storageList.append({"ip": ip, "zid": zid})
					ipSet.add(ip)
				except socket.error:
					raise Exception("[deploy] contains an invalid ip %s"%ip)
				except ValueError:
					raise Exception("[deploy] contains an invalid zid %s"%tokens[1])
		return (proxyList, storageList)
	except IOError as e:
		msg = "Failed to access input files for %s"%str(e)
		raise Exception(msg)

def addNodes():
        '''
        Command line implementation of adding swift nodes
        '''
        ret = 1

        inputFile = "/etc/delta/inputFile"
        try:

                (proxyList, storageList) = parseAddNodesSection(inputFile=inputFile)

                SD = SwiftDeploy()
                t = Thread(target=SD.addNodes, args=(proxyList, storageList))
                t.start()

                print "Updating Metadata..."
                progress = SD.getUpdateMetadataProgress()
                while progress['finished'] != True:
                        time.sleep(10)
                        progress = SD.getUpdateMetadataProgress()

                if progress['code'] !=0:
                        print "Failed to update metadata for %s"%progress["message"]
                        sys.exit(1)

                print "Deploy new nodes..."
                progress = SD.getDeployProgress()
                while progress['finished'] != True:
                        time.sleep(10)
                        progress = SD.getDeployProgress()
                        print progress

                print "Spread ring files..."
                spreadProgress = SD.getSpreadProgress()
                while spreadProgress['finished'] != True:
                        time.sleep(8)
                        spreadProgress = SD.getSpreadProgress()
                        print  spreadProgress

                return 0
        except Exception as e:
                print >>sys.stderr, str(e)
	finally:
                return ret

def deleteNodes():
        '''
        Command line implementation of deleting swift nodes
        '''
        ret = 1

        inputFile = "/etc/delta/inputFile"
        try:

                (proxyList, storageList) = parseDeleteNodesSection(inputFile=inputFile)
                print proxyList, storageList

                SD = SwiftDeploy()
                t = Thread(target=SD.deleteNodes, args=(proxyList, storageList))
                t.start()

                print "Updating Metadata..."
                progress = SD.getUpdateMetadataProgress()
                while progress['finished'] != True:
                        time.sleep(10)
                        progress = SD.getUpdateMetadataProgress()

                if progress['code'] !=0:
                        print "Failed to update metadata for %s"%progress["message"]
                        sys.exit(1)

                print "Spread ring files..."
                spreadProgress = SD.getSpreadProgress()
                print spreadProgress
                while spreadProgress['finished'] != True:
                        time.sleep(8)
                        print  spreadProgress
                        spreadProgress = SD.getSpreadProgress()

                print "Clean nodes..."
                cleanProgress = SD.getCleanProgress()
                print cleanProgress
                while cleanProgress['finished'] != True:
                        time.sleep(8)
                        cleanProgress = SD.getCleanProgress()
                        print cleanProgress

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

	Usage = '''
	Usage:
		dcloud_swift_deploy numOfReplica
	arguments:
		numOfReplica: a positive integer less than 10
	Examples:
		dcloud_swift_deploy 3
	'''

	if (len(sys.argv) != 2) or not sys.argv[1].isdigit():
		print >>sys.stderr, Usage
		sys.exit(1)

	numOfReplica = int(sys.argv[1])
	if numOfReplica <1 or numOfReplica > 9:
		print >>sys.stderr, Usage
                sys.exit(1)

	inputFile = "/etc/delta/inputFile"
	try:
						
		(proxyList, storageList) = parseDeploySection(inputFile)

		SC = SwiftCfg(GlobalVar.SWIFTCONF)
		password = SC.getKwparams()['password']

		SD = SwiftDeploy()
		t = Thread(target=SD.deploySwift, args=(proxyList, storageList, numOfReplica))
		t.start()

		print "Creating Metadata..."
		progress = SD.getUpdateMetadataProgress()
		while progress['finished'] != True:
			time.sleep(10)
			progress = SD.getUpdateMetadataProgress()

		if progress['code'] !=0:
			print "Failed to create metadata for %s"%progress["message"]
			sys.exit(1)

		progress = SD.getDeployProgress()
		while progress['finished'] != True:
			time.sleep(8)
			print progress
			progress = SD.getDeployProgress()

		if progress['code'] == 0:
			print "Swift deploy process is done!"
			#create a default account:user
			print "Create a default user..."
			cmd = "swauth-prep -K %s -A https://%s:8080/auth/"%(password, proxyList[0]["ip"])	
			os.system(cmd)	
			os.system("swauth-add-user -A https://%s:8080/auth -K %s -a system root testpass"% (proxyList[0]["ip"], password))
		else:
			print "Swift deploy failed"

		return 0
	except Exception as e:
		print >>sys.stderr, str(e)
	finally:
		return ret

if __name__ == '__main__':
	print util.getNumOfReplica("/etc/delta/swift")
	#t = Thread(target=SD.cleanNodes, args=(["172.16.229.132"],))
	#t = Thread(target=SD.spreadRingFiles, args=())
	#t.start()
	#progress = SD.getCleanProgress()
	#while progress['finished'] != True:
	#	time.sleep(10)
	#	print progress
	#	progress = SD.getCleanProgress()

	
	#spreadProgress = SD.getSpreadProgress()
	#util.spreadPackages(password="deltacloud", nodeList=["172.16.229.122", "172.16.229.34", "172.16.229.46", "172.16.229.73"])
	#util.spreadRC(password="deltacloud", nodeList=["172.16.229.122"])
	#SD = SwiftDeploy([{"ip":"172.16.229.82"}], [{"ip":"172.16.229.145", "zid":1}])
	#SD = SwiftDeploy([{"ip":"172.16.229.35"}], [{"ip":"172.16.229.146", "zid":1}, {"ip":"172.16.229.35", "zid":2}])
	
