import sys
import os
import socket
import posixfile
import time
import json
import subprocess
import shlex
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
os.chdir(WORKING_DIR)
sys.path.append("%s/DCloudSwift/util"%BASEDIR)

import StorageInstall
import util

EEXIST = 17
lockFile = "/etc/delta/swift.lock"


Usage = '''
Usage:
	python CmdReceiver.py [Option] jsonStr
Options:
	[-s | storage] - for storage node
Examples:
	python CmdReceiver.py -s {"password": "deltacloud"}
'''

class UsageError(Exception):
	pass

def usage():
	print >> sys.stderr, Usage

def triggerStorageDeploy(**kwargs):
	proxy = kwargs['proxyList'][0]["ip"]
	proxyList = kwargs['proxyList']

	devicePrx = kwargs['devicePrx']
	deviceCnt = kwargs['deviceCnt']
	installer = StorageInstall.StorageNodeInstaller(proxy=proxy, proxyList=proxyList, devicePrx=devicePrx, deviceCnt=deviceCnt)
	installer.install()
	
def main():
	
	returncode =0
	fd = -1
	try:
		os.system("mkdir -p %s"%os.path.dirname(lockFile))
		fd = os.open(lockFile, os.O_RDWR| os.O_CREAT | os.O_EXCL, 0444)

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

		if (len(sys.argv) == 3 ):
			kwargs = None
			if (sys.argv[1] == 'storage' or sys.argv[1] == '-s'):
				kwargs = json.loads(sys.argv[2])
				print 'storage deployment start'
				triggerStorageDeploy(**kwargs)
			else:
				print >> sys.stderr, "Usage error: Invalid optins"
				raise UsageError

        	else:
			raise UsageError
	except OSError as e:
		if e.errno == EEXIST:
			print >>sys.stderr, "A confilct task is in execution"
		else:
			print >>sys.stderr, str(e)
		returncode = e.errno
	except UsageError:
		usage()
		returncode =1
	except ValueError:
		print >>sys.stderr,  "Usage error: Ivalid json format"
		returncode = 1
	except Exception as e:
		print >>sys.stderr, str(e)
		returncode = 1
	finally:
		if fd != -1:
			os.close(fd)
			os.unlink(lockFile)
		sys.exit(returncode)


if __name__ == '__main__':
	main()
	print "End of executing CmdReceiver.py"
