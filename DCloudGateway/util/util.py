import os, sys, subprocess
import fcntl
import logging
import logging.handlers
import threading
import random
import socket
import struct
import math
import pickle
import time

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))

from ConfigParser import ConfigParser

CONF = '%s/Gateway.ini'%BASEDIR
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
		def wrapper(*args, **kwargs):
			mtries, mdelay = tries, delay # make mutable
			rv = f(*args, **kwargs) # first attempt
			while mtries > 0:
				if rv ==0 or rv ==True: # Done on success
					return rv
				mtries -= 1      # consume an attempt
				time.sleep(mdelay) # wait...
				rv = f(*args, **kwargs) # Try again
  			return rv # Ran out of tries
  		return wrapper #decorated function
  	
  	return deco_retry  # @retry(arg[, ...]) -> true decorator

#timeout decorator
def timeout(timeout_time):
	def timeoutDeco(f):
		def wrapper(*args,**kwargs):
			class InterruptableThread(threading.Thread):
				def __init__(self,f, *args, **kwargs):
            				threading.Thread.__init__(self)
					self.f = f
            				self.args =args
					self.kwargs = kwargs
					self.result = None
        			def run(self):
					result = None
					try:
						self.result = (0,self.f(*(self.args), **(self.kwargs)))
						
					except Exception as e:
						self.result = (1, e)
		
			timeout=timeout_time
			if timeout <=0:
				timeout = 86400*7 #one week
			it = InterruptableThread(f, *args, **kwargs)
			it.daemon =True
			it.start()
			it.join(timeout)
			if it.isAlive():
				raise TimeoutError(timeout)
			elif it.result[0] == 0:
				return it.result[1]
			else:
				raise e

		return wrapper
	return timeoutDeco

#TODO: findout a beter way to check if a daemon is alive
def isDaemonAlive(daemonName):
	logger = getLogger(name="isDaemonAlive")

	cmd = 'ps -ef | grep %s'%daemonName
	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	lines = po.stdout.readlines()
	po.wait()

	if po.returncode !=0:
		return False

	if len(lines) > 2:
		return True
	else: 
		return False
	

def runPopenCommunicate(cmd, inputString, logger):
	po = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
	(stdout, stderr) = po.communicate(inputString)

	if po.returncode == 0:
		logger.debug("Succeed to run \'%s\'"%cmd)
	else:
		logger.error(stderr)

	return po.returncode

class GatewayCfg:
	def __init__(self, configFile):
		self.__kwparams = {}
		self.__configFile = configFile

	def get(self, section, name, value):
		pass

	def set(self, section, name, value):
		pass

def getLogger(name=None, conf=CONF):
	"""
	Get a file logger using config settings.

	"""
	
	try:

		logger = logging.getLogger(name)

		if not hasattr(getLogger, 'handler4Logger'):
			getLogger.handler4Logger = {}

		if logger in getLogger.handler4Logger:
			return logger

		kwparams = GatewayCfg(conf).getKwparams()
	
		logDir = kwparams.get('logDir', '/var/log/deltaGateway/')
		logName = kwparams.get('logName', 'deltaGateway.log')
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

def getIpAddress():
	logger = getLogger(name="getIpAddress")
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


class TimeoutError(Exception):
	def __init__(self, cmd=None, timeout=None):
		self.cmd = cmd
		self.timeout = timeout
		if self.timeout is not None:
			self.timeout= str(self.timeout)
	def __str__(self):
		if self.cmd is not None and self.timeout is not None:
			return "Failed to complete \"%s\" in %s secondssss"%(self.cmd, self.timeout)
		elif self.cmd is not None:
			return "Failed to complete in \"%s\" seconds"%self.cmd
		elif self.timeout is not None:
			return "Failed to finish in %s seconds"%(self.timeout)
		else:
			return "TimeoutError"

if __name__ == '__main__':
	pass	
