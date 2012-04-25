import os
import subprocess
import fcntl
import logging
import logging.handlers
import threading
import random
import sys
import signal
import time
import socket
import struct
import math
import pickle

from ConfigParser import ConfigParser
from SwiftCfg import SwiftCfg

SWIFTCONF = '/DCloudSwift/Swift.ini'
FORMATTER = '[%(levelname)s from %(name)s on %(asctime)s] %(message)s'


# Retry decorator
def retry(tries, delay=3):
	'''Retries a function or method until it returns True.
	delay sets the delay in seconds, and backoff sets the factor by which
	the delay should lengthen after each failure. tries must be at least 0, and delay
	greater than 0.'''
	
	tries = math.floor(tries)
	if tries < 0:
		raise ValueError("tries must be 0 or greater")
	if delay <= 0:
		raise ValueError("delay must be greater than 0")
	
	def deco_retry(f):
		def f_retry(*args, **kwargs):
			mtries, mdelay = tries, delay # make mutable
			rv = f(*args, **kwargs) # first attempt
			while mtries > 0:
				if rv ==0 or rv ==True: # Done on success
					return rv
				mtries -= 1      # consume an attempt
				time.sleep(mdelay) # wait...
				rv = f(*args, **kwargs) # Try again
  			return rv # Ran out of tries :-(
  		return f_retry # true decorator -> decorated function
  	
  	return deco_retry  # @retry(arg[, ...]) -> true decorator

#timeout decorator
def timeout(timeout_time, default):
	def timeout_function(f):
		def f2(*args,**kwargs):
			def timeout_handler(signum, frame):
				raise TimeoutError(timeout=timeout_time)
 
			old_handler = signal.signal(signal.SIGALRM, timeout_handler) 
			signal.alarm(timeout_time) # triger alarm in timeout_time seconds
			try: 
				retval = f(*args, **kwargs)
			except TimeoutError:
				return default
			finally:
				signal.signal(signal.SIGALRM, old_handler) 
			signal.alarm(0)
			return retval
		return f2
	return timeout_function

def restartRsync():
	
	os.system("/etc/init.d/rsync stop")
	os.system("rm /var/run/rsyncd.pid")

	cmd = "/etc/init.d/rsync start"
	po = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)

	output = po.stdout.read()
	po.wait()

	return po.returncode

def restartMemcached():
	
	os.system("/etc/init.d/memcached stop")
	os.system("rm /var/run/memcached.pid")

	cmd = "/etc/init.d/memcached start 1>/dev/null 2>/dev/null &"
	po = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)

	output = po.stdout.read()
	po.wait()

	return po.returncode

def restartSwiftServices():
	'''
	start appropriate swift services
	'''
	pass


def runPopenCommunicate(cmd, inputString, logger):
	po = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
	(stdout, stderr) = po.communicate(inputString)

	if po.returncode == 0:
		logger.debug("Succeed to run \'%s\'"%cmd)
	else:
		logger.error(stderr)

	return po.returncode

def findLine(filename, line):
	f = open(filename, 'r')
	for l in f.readlines():
		l1 = l.strip()
		l2 = line.strip()
		if l1 == l2:
			return True

	return False
	
def getLogger(name=None, conf=SWIFTCONF):
	"""
	Get a file logger using config settings.

	"""
	
	try:

		logger = logging.getLogger(name)

		if not hasattr(getLogger, 'handler4Logger'):
			getLogger.handler4Logger = {}

		if logger in getLogger.handler4Logger:
			return logger

		kwparams = SwiftCfg(conf).getKwparams()
	
		logDir = kwparams.get('logDir', '/var/log/deltaSwift/')
		logName = kwparams.get('logName', 'deltaSwift.log')
		logLevel = kwparams.get('logLevel', 'INFO')

		os.system("mkdir -p "+logDir)
		os.system("touch "+logDir+'/'+logName)

		hdlr = logging.handlers.RotatingFileHandler(logDir+'/'+logName, maxBytes=1024*1024, backupCount=5)
		hdlr.setFormatter(logging.Formatter(FORMATTER))
		logger.addHandler(hdlr)
		logger.setLevel(logLevel)
		logger.propagate = False	

		getLogger.handler4Logger[logger] = hdlr
		return logger
	finally:
		pass

def generateSwiftConfig():
	ip = socket.gethostbyname(socket.gethostname())
	if ip.startswith("127"):
		ip =getIpAddress()

	os.system("/DCloudSwift/proxy/CreateProxyConfig.sh %s"%ip)
	os.system("/DCloudSwift/storage/rsync.sh %s"%ip)
	os.system("/DCloudSwift/storage/accountserver.sh %s"%ip)
	os.system("/DCloudSwift/storage/containerserver.sh %s"%ip)
	os.system("/DCloudSwift/storage/objectserver.sh %s"%ip)

	os.system("chown -R swift:swift /etc/swift")

def getIpAddress():
	ipaddr = socket.gethostbyname(socket.gethostname())
	if not ipaddr.startswith("127"):
		return ipaddr

    	arg='ip route list'    
    	p=subprocess.Popen(arg,shell=True,stdout=subprocess.PIPE)
    	data = p.communicate()
    	sdata = data[0].split()
    	ipaddr = sdata[ sdata.index('src')+1 ]
    	#netdev = sdata[ sdata.index('dev')+1 ]
    	return ipaddr


def getSwiftNodeIpList():
	storageIpList = getStorageNodeIpList()
	proxyList =[]
	ipSet = set()
	
	try:
		with open("/etc/swift/proxyList","rb") as fh:
			proxyList = pickle.load(fh)
	except IOError:
		logger.error("Failed to load proxyList")

	for ip in storageIpList:
		ipSet.add(ip)

	for node in proxyList:
		ipSet.add(node["ip"])

	ipList = [ip for ip in ipSet]
	
	return ipList

def getSwiftConfVers(confDir="/etc/swift"):
	logger = getLogger(name="getSwiftConfVers")

	cmd = 'cd %s; swift-ring-builder object.builder'%confDir
	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	lines = po.stdout.readlines()
	po.wait()

	if po.returncode !=0:
		logger.error("Failed to get version info from %s/objcet.builder"%confDir)
		return -1
		
	tokens = lines[0].split()	
	vers = int(tokens[3])
	
	versBase = 0
	try:
		with open("%s/versBase"%confDir, "rb") as fh:
			versBase = pickle.load(fh)
	except OSError:
		logger.erro("Failed to load version base from %s/versBase"%confDir)
		return -1
	
	return vers+versBase
	

def getStorageNodeIpList():
	'''
	Collect ip list of all storge nodes  
	'''

	logger = getLogger(name='SwiftInfo')

	cmd = 'cd /etc/swift; swift-ring-builder object.builder'
	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	lines = po.stdout.readlines()
	po.wait()
	
	if po.returncode != 0:
		return None

	i = 0
	ipList =[]
	for line in lines:
		if i > 3:
			ipList.append(line.split()[2])

		i+=1

	return ipList


def sshpass(passwd, cmd, timeout=0):

	def timeoutHandler(signum, frame):
		raise TimeoutError()
 
	old_handler = signal.signal(signal.SIGALRM, timeoutHandler) 
	signal.alarm(timeout) # triger alarm in timeout_time seconds
	
	try:

		sshpasscmd = "sshpass -p %s %s" % (passwd, cmd)
		po  = subprocess.Popen(sshpasscmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		
		(stdoutData, stderrData) = po.communicate()
		
		return (po.returncode, stdoutData, stderrData)
		
	except TimeoutError:
		raise TimeoutError(cmd=cmd, timeout=timeout)
	finally:
		signal.alarm(0)
		signal.signal(signal.SIGALRM, old_handler)


def sendMaterials(password, peerIp):
	logger = getLogger(name="sendMaterials")
	logger.info("start")

	myIp = getIpAddress()
	returncode = 1

	try:
		cmd = "ssh root@%s mkdir -p /etc/delta/%s"%(peerIp, myIp)
		(status, stdout, stderr) = sshpass(password, cmd, timeout=20)
		if status != 0:
			raise SshpassError(stderr)
		
		os.system("mkdir -p /etc/delta/swift")
		os.system("cp -r /etc/swift/* /etc/delta/swift/")

		#TODO: delete unnecessary files

		logger.info("scp -r -o StrictHostKeyChecking=no --preserve /etc/delta/swift/ root@%s:/etc/delta/%s/"%(peerIp, myIp))
		cmd = "scp -r -o StrictHostKeyChecking=no --preserve /etc/delta/swift/ root@%s:/etc/delta/%s/"%(peerIp, myIp)
		(status, stdout, stderr) = sshpass(password, cmd, timeout=120)
		if status !=0:
			raise SshpassError(stderr)

			
		logger.info("scp -r -o StrictHostKeyChecking=no --preserve /DCloudSwift/ root@%s:/etc/delta/%s/"%(peerIp, myIp))
		cmd = "scp -r -o StrictHostKeyChecking=no --preserve /DCloudSwift/ root@%s:/etc/delta/%s/"%(peerIp, myIp)
		(status, stdout, stderr) = sshpass(password, cmd, timeout=120)
		if status !=0:
			raise SshpassError(stderr)

		returncode =0
	except TimeoutError as err:
		logger.error("Failed to execute \"%s\" in time"%(cmd)) 
	except SshpassError as err:
		logger.error("Failed to execute \"%s\" for %s"%(cmd, err))
	finally:
		logger.info("end")
		return returncode
	
	
def spreadMetadata(password, sourceDir="/etc/swift/", nodeList=[]):
	logger = getLogger(name="spreadMetadata")
	blackList=[]
	returncode = 0
	for ip in nodeList:
		try:
			cmd = "ssh root@%s mkdir -p /etc/swift/"%ip
			(status, stdout, stderr) = sshpass(password, cmd, timeout=20)
			if status != 0:
				raise SshpassError(stderr)

			logger.info("scp -o StrictHostKeyChecking=no --preserve %s/* root@%s:/etc/swift/"%(sourceDir, ip))
			cmd = "scp -o StrictHostKeyChecking=no --preserve %s/* root@%s:/etc/swift/"%(sourceDir, ip)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=360)
			if status !=0:
				raise SshpassError(stderr)


			cmd = "ssh root@%s chown -R swift:swift /etc/swift "%(ip)

			(status, stdout, stderr) = sshpass(password, cmd, timeout=20)
			if status != 0:
				raise SshpassError(stderr)

		except TimeoutError as err:
			blackList.append(ip)
			returncode +=1
			logger.error("Failed to execute \"%s\" in time"%(cmd)) 
			continue
		except SshpassError as err:
			blackList.append(ip)
			returncode +=1
			logger.error("Failed to execute \"%s\" for %s"%(cmd, err))
			continue
					

	return (returncode, blackList)

def spreadPackages(password, nodeList=[]):
	logger = getLogger(name="spreadPackages")
	blackList=[]
	returncode = 0
	cmd =""
	for ip in nodeList:
		try:
			print "Start installation of swfit packages on %s ..."%ip

			cmd = "ssh root@%s mkdir -p /etc/lib/swift/"%(ip)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=60)
                        if status != 0:
                                raise SshpassError(stderr)

			logger.info("scp -o StrictHostKeyChecking=no -r /etc/lib/swift/* root@%s:/etc/lib/swift/"%(ip))
			cmd = "scp -o StrictHostKeyChecking=no -r /etc/lib/swift/* root@%s:/etc/lib/swift/"%(ip)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=60)
			if status !=0:
				raise SshpassError(stderr)


			cmd = "ssh root@%s dpkg -i /etc/lib/swift/*.deb "%(ip)

			(status, stdout, stderr) = sshpass(password, cmd, timeout=360)
			if status != 0:
				raise SshpassError(stderr)

		except TimeoutError as err:
			blackList.append(ip)
			returncode +=1
			logger.error("Failed to execute \"%s\" in time"%(cmd)) 
			print "Failed to install swift packages on %s"%ip
			continue
		except SshpassError as err:
			blackList.append(ip)
			returncode +=1
			logger.error("Failed to execute \"%s\" for %s"%(cmd, err))
			print "Failed to install swift packages on %s"%ip
			continue
					

	return (returncode, blackList)
	
def spreadRC(password, nodeList=[]):
	logger = getLogger(name="spreadRC")
	blackList=[]
	returncode = 0
	cmd=""
	for ip in nodeList:
		try:
			print "Start spreading rc.local to %s ..."%ip


			logger.info("scp -o StrictHostKeyChecking=no /etc/lib/swift/BootScripts/rc root@%s:/etc/init.d/rc"%(ip))
			cmd = "scp -o StrictHostKeyChecking=no /etc/lib/swift/BootScripts/rc root@%s:/etc/init.d/rc"%(ip)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=60)
			if status !=0:
				raise SshpassError(stderr)


		except TimeoutError as err:
			blackList.append(ip)
			returncode +=1
			logger.error("Failed to execute \"%s\" in time"%(cmd)) 
			print "Failed to spread rc.local to %s"%ip
			continue
		except SshpassError as err:
			blackList.append(ip)
			returncode +=1
			logger.error("Failed to execute \"%s\" for %s"%(cmd, err))
			print "Failed to rc.local to %s"%ip
			continue
					

	return (returncode, blackList)

def jsonStr2SshpassArg(jsonStr):
	arg = jsonStr.replace(" ","")
	arg = arg.replace("{","\{")
	arg = arg.replace("}","\}")
	arg = arg.replace("\"", '\\"')
	arg = "\'"+arg+"\'"
	return arg
	

class TimeoutError(Exception):
	def __init__(self, cmd=None, timeout=None):
		self.cmd = cmd
		self.timeout= str(timeout)
	def __str__(self):
		if cmd is not None and timeout is not None:
			return "Failed to complete \"%s\" in %s seconds"%(self.cmd, self.timeout)
		elif cmd is not None:
			return "Failed to complete \"%s\" in time"%self.cmd
		elif timeout is not None:
			return "Failed to finish in %s seconds"%(sefl.timeout)
		else:
			return "TimeoutError"

class SshpassError(Exception):
	def __init__(self, errMsg):
		self.errMsg = errMsg
	def __str__(self):
		return self.errMsg
		

if __name__ == '__main__':
#	print getSwiftConfVers()
#	print jsonStr2SshpassArg('{ "Hello" : 3, "list":["192.167.1.1", "178.16.3.1"]}')
#	spreadPackages(password="deltacloud", nodeList = ["172.16.229.24"])
#	print installAllDeb("/DCloudSwift/storage/deb_source")
#	print isLineExistent("/etc/fstab","ddd")
#	print getStorageNodeIpList()
#	runPopenCommunicate("cd /etc/swift", inputString='y\n', logger=logger)
#	runPopenCommunicate("mkfs -t ext4 /dev/sda", inputString='y\n', logger=logger)

#	logger = getLogger(name="Hello")
#	logger.info("Hello")

#	spreadMetadata(password="deltacloud",nodeList=["172.16.229.132"])

#	@timeout(5, "This is timeout!!!")
#	def printstring():
#		print "Start!!"
#		time.sleep(10)
#		print "This is not timeout!!!"

#	s = printstring()
#	print s

	sendMaterials("deltacloud", "172.16.229.146")
