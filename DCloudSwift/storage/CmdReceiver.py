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
	python CmdReceiver.py [Option]
Options:
	[-s | storage] - for storage node
Examples:
	python CmdReceiver.py -s 
'''

def usage():
	print Usage
	sys.exit(1)

def triggerStorageDeploy(**kwargs):
	proxyNode = kwargs['proxyList'][0]
	devicePrx = kwargs['devicePrx']
	deviceCnt = kwargs['deviceCnt']
	installer = StorageInstall.StorageNodeInstaller(proxyNode, devicePrx, deviceCnt)
	installer.install()
	
def main():
	if (len(sys.argv) == 2 ):
		kwargs = None
		if (sys.argv[1] == 'storage' or sys.argv[1] == '-s'):
			f = file('/DCloudSwift/storage/StorageParams', 'r')
			kwargs = f.readline()

			try:
				kwargs = json.loads(kwargs)
			except ValueError:
				print "Usage error: Ivalid json format"
				usage()

			print 'storage deployment start'
			triggerStorageDeploy(**kwargs)
		else:
			print "Usage error: Invalid optins"
                	usage()
        else:
		#print len(sys.argv)
		#for i in range(0, len(sys.argv)):
		#	print sys.argv[i]
		usage()


if __name__ == '__main__':
	main()
	print "End of executing CmdReceiver.py"
