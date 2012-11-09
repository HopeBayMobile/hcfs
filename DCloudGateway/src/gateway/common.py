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
import urllib2
import urllib
import json

from ConfigParser import ConfigParser

CAFEPOSTLOG = "http://127.0.0.1:80/restful/services/cafelog/post"
CAFEGETLOG = "http://127.0.0.1:80/restful/services/cafelog/getlist"
CAFEDELETELOG = "http://127.0.0.1:80/restful/services/cafelog/delete"

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


class CosaLogger :

    def __init__(self, module, post_url, get_url, delete_url):
        self.post_url = post_url
        self.get_url = get_url
        self.delete_url = delete_url
        self.module = module

    def __log(self, level,  message):
        values = {"module": self.module, "level": level, "message": message}
        data = json.dumps(values)
        req = urllib2.Request(self.post_url, data, {'Content-Type': 'application/json'})
        code = -1
        response = None

        try:
            f = urllib2.urlopen(req)
            code = f.getcode()
            response = f.read()
            f.close()
        except urllib2.HTTPError as e:
            code = e.code
            response = e.read()
        except urllib2.URLError as e:
            response = e.reason
        except Exception as e:
            response = str(e)

        return {"code": code, "response": response}

    def get_list(self):
        req = urllib2.Request(self.get_url)
        code = -1
        response = None

        try:
            f = urllib2.urlopen(req)
            code = f.getcode()
            response = f.read()
            f.close()
        except urllib2.HTTPError as e:
            code = e.code
            response = e.read()
        except urllib2.URLError as e:
            response = e.reason
        except Exception as e:
            response = str(e)

        return {"code": code, "response": response}

    def debug(self, message):
        return self.__log(level='DEBUG', message=message)

    def info(self, message):
        return self.__log(level='INFO', message=message)

    def warning(self, message):
        return self.__log(level='WARNING', message=message)

    def error(self, message):
        return self.__log(level='ERROR', message=message)

    def critical(self, message):
        return self.__log(level='CRITICAL', message=message)

def getLogger(name=None, conf=None):
	"""
	Get a file logger using config settings.

	"""
        # TODO: read url from config
        if name:
            logger = CosaLogger(module=name, 
                                post_url=CAFEPOSTLOG,
                                get_url=CAFEGETLOG,
                                delete_url=CAFEDELETELOG,
                               )
        else:
            logger = CosaLogger(module="Gateway", 
                                post_url=CAFEPOSTLOG,
                                get_url=CAFEGETLOG,
                                delete_url=CAFEDELETELOG,
                               )

        return logger
	

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

####################################################################
# Daemonize process
def daemonize(workdir='/'):
    """
    Daemonize the process
    """

    os.chdir(workdir)

    detach_process_context()

    # wthung, disable redirection because after redirection,
    #   some eror occurred when calling sudo by subprocess.popen
    redirect_stream(sys.stdin, None)
    redirect_stream(sys.stdout, None)
    redirect_stream(sys.stderr, None)


def detach_process_context():
    """
    Detach the process context from parent and session.

    Detach from the parent process and session group, allowing the
    parent to exit while this process continues running.
    """
    pid = os.fork()
    if pid > 0:
        os._exit(0)

    os.setsid()

    pid = os.fork()
    if pid > 0:
        os._exit(0)


def redirect_stream(system_stream, target_stream):
    """ 
    Redirect a system stream to a specified file.

    `system_stream` is a standard system stream such as
    ``sys.stdout``. `target_stream` is an open file object that
    should replace the corresponding system stream object.

    If `target_stream` is ``None``, defaults to opening the
    operating system's null device and using its file descriptor.
    """
    if target_stream is None:
        target_fd = os.open(os.devnull, os.O_RDWR)
    else:
        target_fd = target_stream.fileno()
    os.dup2(target_fd, system_stream.fileno())
##############################################################################

if __name__ == '__main__':
        logger = CosaLogger(module="Gateway", 
                            post_url=CAFEPOSTLOG,
                            get_url=CAFEGETLOG,
                            delete_url=CAFEDELETELOG,
                           )
        print logger.warning("hello")
        print logger.get_list()
	pass	
