import os
import subprocess
import logging
import threading
import sys
import signal
import time
import socket
import math

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
				raise TimeoutError(time=str(timeout_time))
 
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

def startSwiftServices(():
	'''
	start appropriate swift services
	'''


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

		hdlr = logging.FileHandler(logDir+'/'+logName)
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

	os.system("/DCloudSwift/proxy/CreateProxyConfig.sh %s"%ip)

	os.system("/DCloudSwift/storage/rsync.sh %s"%ip)

	os.system("/DCloudSwift/storage/accountserver.sh %s"%ip)
	os.system("/DCloudSwift/storage/containerserver.sh %s"%ip)
	os.system("/DCloudSwift/storage/objectserver.sh %s"%ip)

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
		raise TimeoutError(cmd=cmd, timeout=str(timeout))
	finally:
		signal.alarm(0)
		signal.signal(signal.SIGALRM, old_handler)

	
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


			logger.info("scp -o StrictHostKeyChecking=no /etc/lib/swift/BootScripts/rc.local root@%s:/etc/rc.local"%(ip))
			cmd = "scp -o StrictHostKeyChecking=no /etc/lib/swift/BootScripts/rc.local root@%s:/etc/rc.local"%(ip)
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
		self.timeout= timeout
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
#	logger = getLogger(name="Hello")
#	runPopenCommunicate("cd /etc/swift", inputString='y\n', logger=logger)
#	runPopenCommunicate("mkfs -t ext4 /dev/sda", inputString='y\n', logger=logger)

#	logger2 = getLogger(name="Hello")
#	logger2.info("Hello2")

#	logger3 = getLogger(name="Hi")
#	logger3.info("Hi")

#	try:
#		cmd = "ssh root@192.168.1.131 touch aaa"
#		(returncode, stdoutdata) = sshpass("deltacloud", cmd, timeout=2)
#		print stdoutdata.readlines()
#	except TimeoutError as err:
#		print err
#	spreadMetadata(password="deltacloud",nodeList=["192.168.1.132"])
#	pass
	@timeout(5, "This is timeout!!!")
	def printstring():
		print "Start!!"
		time.sleep(10)
		print "This is not timeout!!!"

	s = printstring()
	print s
