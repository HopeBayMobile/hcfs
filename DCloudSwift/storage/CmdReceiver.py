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


import StorageInstall

Usage = '''
Usage:
	python CmdReceiver.py [Option] jsonStr
Options:
	[-s | storage] - for storage node
Examples:
	python CmdReceiver.py -s {"password": "deltacloud"}
'''

def usage():
	print >> sys.stderr, Usage
	sys.exit(1)

def triggerStorageDeploy(**kwargs):
	proxyNode = kwargs['proxyList'][0]

	devicePrx = kwargs['devicePrx']
	deviceCnt = kwargs['deviceCnt']
	installer = StorageInstall.StorageNodeInstaller(proxyNode, devicePrx, deviceCnt)
	installer.install()
	
def main():
	if (len(sys.argv) == 3 ):
		kwargs = None
		if (sys.argv[1] == 'storage' or sys.argv[1] == '-s'):
			try:
				kwargs = json.loads(sys.argv[2])
			except ValueError:
				sys.stderr.write( "Usage error: Ivalid json format\n")
				usage()

			print 'storage deployment start'
			triggerStorageDeploy(**kwargs)
		else:
			sys.stderr.write( "Usage error: Invalid optins\n")
                	usage()
        else:
		usage()


if __name__ == '__main__':
	main()
	print "End of executing CmdReceiver.py"
