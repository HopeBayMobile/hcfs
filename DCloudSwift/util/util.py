import os
import subprocess
import logging
import threading
import sys

from SwiftCfg import SwiftCfg

SWIFTCONF = '../Swift.ini'
FORMATTER = '[%(levelname)s from %(name)s on %(asctime)s] %(message)s'

logLock = threading.Lock()

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

		os.system("sudo mkdir -p "+logDir)
		os.system("sudo touch "+logDir+'/'+logName)

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

	logger = logging.getLogger('SwiftInfo')
	hdlr = logging.FileHandler('/var/swiftInfo.log')
	formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
	hdlr.setFormatter(formatter)
	logger.addHandler(hdlr)
	logger.setLevel(logging.INFO)

	logger.info('a log message')

	cmd = 'cd /etc/swift; swift-ring-builder object.builder'
	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	po.wait()

	i = 0
	ipList =[]
	for line in po.stdout.readlines():
		if i > 4:
			ipList.append(line.split()[2])

		i+=1

	return ipList


if __name__ == '__main__':
#	print getStorageNodeIpList()

	logger = getLogger(name="Hello")
	logger.info("HELLo!")

	logger2 = getLogger(name="Hello")
	logger2.info("Hello2")

	logger3 = getLogger(name="Hi")
	logger3.info("Hi")

	
