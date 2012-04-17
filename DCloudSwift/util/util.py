'''
Created on 2012/03/08

@author: Ken

Modified by Ken, Mingchi on 2012/03/09
Modified by Ken on 2012/03/28
'''
import os
import subprocess
import logging
import threading
import sys
import time

from SwiftCfg import SwiftCfg

SWIFTCONF = '/DCloudSwift/Swift.ini'
FORMATTER = '[%(levelname)s from %(name)s on %(asctime)s] %(message)s'

logLock = threading.Lock()

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
	
def getLogger(conf=SWIFTCONF, name=None):
	"""
	Get a file logger using config settings.

	"""
	
	try:
		logLock.acquire()

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
		logLock.release()


def getStorageNodeIpList():
	'''
	Collect ip list of all storge nodes  
	'''

	logger = getLogger(name='SwiftInfo')

	cmd = 'cd /etc/swift; swift-ring-builder object.builder'
	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	po.wait()

	i = 0
	ipList =[]
	for line in po.stdout.readlines():
		if i > 3:
			ipList.append(line.split()[2])

		i+=1

	return ipList

def isAllDebInstalled(debSrc):
	logger = getLogger(name='isAllDebInstalled')
	cmd = "find %s -maxdepth 1 -name \'*.deb\'  "%debSrc
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	po.wait()

	if po.returncode !=0:
		logger.error("Failed to execute %s for %s"%(cmd, po.stderr.readlines()))
		return -1

	returncode = 0
	devnull = open(os.devnull, "w")
	for line in po.stdout.readlines():
		pkgname = line.split('/')[-1].split('_')[0]
		retval = subprocess.call(["dpkg", "-s", pkgname], stdout=devnull, stderr=devnull)
		if retval != 0:
			return False

	devnull.close()
	
	return True

def installAllDeb(debSrc):
	logger = getLogger(name='installDeb')
	cmd = "dpkg -i  %s/*.deb"%debSrc
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	po.wait()

	if po.returncode !=0:
		logger.error("Failed to execute %s for %s"%(cmd, po.stderr.readlines()))
		return 1

	return 0

def sshpass(passwd, cmd, timeout=0):

	t_beginning = time.time()
	seconds_passed = 0

	sshpasscmd = "sshpass -p %s %s" % (passwd, cmd)
	po  = subprocess.Popen(sshpasscmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

	while True:
		if po.poll() is not None:
			break
		seconds_passed = time.time() - t_beginning
		if timeout and seconds_passed > timeout:
			po.terminate()
			raise TimeoutError(sshpasscmd, timeout)
		time.sleep(0.1)

	return (po.returncode, po.stdout, po.stderr)
	
def spreadMetadata(password, sourceDir="/etc/swift/", nodeList=[]):
	logger = getLogger(name="spreadMetadata")
	blackList=[]
	returncode = 0
	for ip in nodeList:
		try:
			cmd = "ssh root@%s mkdir -p /etc/swift/"%ip
			(status, stdout, stderr) = sshpass(password, cmd, timeout=20)
			if status != 0:
				raise SshpassError(stderr.read())

			logger.info("scp -o StrictHostKeyChecking=no --preserve %s/*.ring.gz %s/*.builder %s/swift.conf root@%s:/etc/swift/"%(sourceDir, sourceDir, sourceDir, ip))
			cmd = "scp -o StrictHostKeyChecking=no --preserve %s/*.ring.gz %s/*.builder root@%s:/etc/swift/"%(sourceDir,sourceDir, ip)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=60)
			if status !=0:
				raise SshpassError(stderr.read())


			cmd = "ssh root@%s chown -R swift:swift /etc/swift "%(ip)

			(status, stdout, stderr) = sshpass(password, cmd, timeout=20)
			if status != 0:
				raise SshpassError(stderr.read())

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
	for ip in nodeList:
		try:
			print "Start installation of swfit packages on %s ..."%ip

			cmd = "ssh root@%s mkdir -p /var/lib/swift/"%(ip)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=60)
                        if status != 0:
                                raise SshpassError(stderr.read())

			logger.info("scp -o StrictHostKeyChecking=no /var/lib/swift/* root@%s:/var/lib/swift/"%(ip))
			cmd = "scp -o StrictHostKeyChecking=no /var/lib/swift/* root@%s:/var/lib/swift/"%(ip)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=60)
			if status !=0:
				raise SshpassError(stderr.read())


			cmd = "ssh root@%s dpkg -i /var/lib/swift/* "%(ip)

			(status, stdout, stderr) = sshpass(password, cmd, timeout=360)
			if status != 0:
				raise SshpassError(stderr.read())

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
	

def jsonStr2SshpassArg(jsonStr):
	arg = jsonStr.replace(" ","")
	arg = arg.replace("{","\{")
	arg = arg.replace("}","\}")
	arg = arg.replace("\"", '\\"')
	arg = "\'"+arg+"\'"
	return arg
	


class TimeoutError(Exception):
	def __init__(self, cmd, timeout):
		self.cmd = cmd
		self.timeout= timeout
	def __str__(self):
		return "Failed to complete \"%s\" in %s seconds"%(self.cmd, self.timeout)

class SshpassError(Exception):
	def __init__(self, errMsg):
		self.errMsg = errMsg
	def __str__(self):
		return self.errMsg
		

if __name__ == '__main__':
#	print jsonStr2SshpassArg('{ "Hello" : 3, "list":["192.167.1.1", "178.16.3.1"]}')
	spreadPackages(password="deltacloud", nodeList = ["172.16.229.34"])
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
	pass
