'''
Created on 2012/03/01

@author: CW
modified by Ken 2012/03/16
'''

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

sys.path.append("/DCloudSwift/util")
import StorageInstall
import util

EEXIST = 17
lockFile = "/tmp/CmdReceiver.lock"


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
	proxyNode = kwargs['proxyList'][0]["ip"]

	devicePrx = kwargs['devicePrx']
	deviceCnt = kwargs['deviceCnt']
	installer = StorageInstall.StorageNodeInstaller(proxyNode, devicePrx, deviceCnt)
	installer.install()
	
def main():
	
	returncode =0
	fd = -1
	try:
		fd = os.open(lockFile, os.O_RDWR| os.O_CREAT | os.O_EXCL, 0444)

		if not util.isAllDebInstalled("/DCloudSwift/storage/deb_source/"):
			util.installAllDeb("/DCloudSwift/storage/deb_source/")

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
