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
import re

from ConfigParser import ConfigParser

#FORMATTER = '[%(levelname)s from %(name)s on %(asctime)s] %(message)s'
FORMATTER = '[%(asctime)s] %(message)s'
DIR = os.path.dirname(os.path.realpath(__file__))

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
					try:
						self.result = (0,self.f(*(self.args), **(self.kwargs)))
						
					except Exception as e:
						self.result = (1, e, sys.exc_info()[2])
		
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
				raise it.result[1], None, it.result[2]

		return wrapper
	return timeoutDeco

def getAllDisks():
        cmd = "sudo smartctl --scan"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()

        disks = []
        for line in lines:
                match = re.match(r"^/dev/sd\w", line)
                if match is not None:
                        disks.append(line.split()[0][:8])

        return disks

# wthung, 2012/7/6
def getDiskCapacity(dev_name):
    """
    Get devicne capacity.

    @type dev_name: string
    @param dev_name: Device name to be queried. For example: /dev/sda.
    @rtype: Float
    @return: Device capacity if any. Otherwise, None.
    """

    cmd = "sudo fdisk -l %s" % dev_name
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    lines = po.stdout.readlines()
    
    for line in lines:
        re1='.*?'	# Non-greedy match on filler
        re2='([+-]?\\d*\\.\\d+)(?![-+0-9\\.])'	# Float 1
        rg = re.compile(re1+re2,re.IGNORECASE|re.DOTALL)
        m = rg.search(line)
        if m:
            float1=m.group(1)
            return float
    
    return None

def getTestFolder():
	config = ConfigParser()
	conf = "%s/test.ini"%DIR
	try:
		with open(conf) as fh:
			config.readfp(fh)
			val = config.get('main', 'folder')
			return val
	except Exception as e:
		return "/"

def isHttp200(response):
	if response.find("HTTP/1.1 200")  !=-1:
		return True
	else:
		return False

def isInTestMode():
	config = ConfigParser()
	conf = "%s/test.ini"%DIR
	try:
		with open(conf) as fh:
			config.readfp(fh)
			val = config.get('main', 'test')
			if val == "on":
				return True
			else:
				return False
	except Exception as e:
		return False

def isValidEncKey(key):
	#Todo: Make sure key is a string

	if key is None:
		return False

	if len(key) > 20 or len(key) <6:
		return False
	
	return key.isalnum()

def getLogger(name=None, conf=None):
	"""
	Get a file logger using config settings.

	"""
	
	try:

		logger = logging.getLogger(name)

		if not hasattr(getLogger, 'handler4Logger'):
			getLogger.handler4Logger = {}

		if logger in getLogger.handler4Logger:
			return logger

		logDir = logName = logLevel = None 

		config = ConfigParser()
		try:
			with open(conf) as fh:
				config.readfp(fh)
				logDir = config.get('log', 'dir')
				logName = config.get('log', 'name')
				logLevel = config.get('log', 'level')
		except Exception as e:
			logDir = '/var/log/delta'
			logName = 'Gateway.log'
			logLevel = 'DEBUG'

		os.system("sudo mkdir -p "+logDir)
		os.system("sudo touch "+logDir+'/'+logName)
                os.system("sudo chown www-data:www-data "+logDir+'/'+logName)

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
	#log = getLogger("test", conf="/etc/delta/Gateway.ini")
	#log.info("TEST")
	@timeout(5)
	def hello():
		raise Exception("GG")

	hello()
	pass	
