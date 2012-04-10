'''
Created on 2011/11/20

@authors: Rice, CW and Ken
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

from GlusterfsDeploy.GlusterdCfg import GlusterdCfg
from GlusterfsDeploy.GlusterfsDeploy import GlusterfsDeploy
from GlusterfsOps.GlusterfsOps import GlusterfsOps

#Third party packages
import paramiko



Usage = '''
Usage:
	cmdReceiver.py [Option] {parameters}
Options:
	[-C | createVolume] - create a new volume
	[-r | readReport] - read the progress report
	[-R | replaceServer] - replace a crashed server 
	[-T | triggerSelfHealing] - trigger self healing
Parameters:
	{'json string'} 
Examples:
	cmdReceiver.py -C '{"hostList":["ntu79", "ntu80"], "volName": "testVol", "volType":"distribute"}' 
'''

ConfigFile = os.path.realpath(os.path.dirname(sys.argv[0]))+"/DCloudGfs.ini"

def usage():
	print Usage
	sys.exit(1)

def createVolume(**kwargs):
	hostList = kwargs['hostList']
	taskId = kwargs['taskId']
	GD = GlusterfsDeploy(ConfigFile,hostList, taskId)

	print taskId
	args = {}

	for key, value in kwargs.iteritems():
		if key == 'hostList' or key == 'taskId':
			continue
		if value is None:
			continue
		args[key] = value

	GD.volumeCreate(**args)

def replaceServer(**kwargs):
	taskId = kwargs['taskId']
	GO = GlusterfsOps(ConfigFile, taskId)

	args = {}

	for key, value in kwargs.iteritems():
		if key == 'taskId':
			continue
		if value is None:
			continue
		args[key] = value

	GO.replaceServer(**args)
	
def triggerSelfHealing(**kwargs):
	taskId = kwargs['taskId']
	GO = GlusterfsOps(ConfigFile, taskId)

	args = {}

	for key, value in kwargs.iteritems():
		if key == 'taskId':
			continue
		if value is None:
			continue
		args[key] = value

	GO.triggerSelfHealing(**args)

def readReport(**kwargs):
	taskId = kwargs['taskId']
	config = ConfigParser()
        config.readfp(open(ConfigFile))

	report = config.get('report', 'dir')+"/"+taskId+"/report"
	print report
	if os.path.isfile(report):
        	fp = posixfile.open(report,'a+')
                fp.lock('w|')
                jsonStr = fp.read()
                fp.lock('u')
		print jsonStr
	else:
		print "{}"

def main():
	if (len(sys.argv) == 3 ):
		kwargs = None
		try:
			kwargs = json.loads(sys.argv[2])
		except ValueError:
			print "Usage error: Ivalid json format"
			usage()
		
        	if (sys.argv[1] == 'createVolume' or sys.argv[1] == '-C'):
			print 'createVolume start'
			createVolume(**kwargs)
		elif (sys.argv[1] == 'readReport' or sys.argv[1] == '-r'):
			print 'readReport start'
			readReport(**kwargs)
		elif (sys.argv[1] == 'replaceServer' or sys.argv[1] == '-R'):
			print 'replaceServer start'
			replaceServer(**kwargs)
		elif (sys.argv[1] == 'triggerSelfHealing' or sys.argv[1] == '-T'):
			print 'triggerSelfHealing start'
			triggerSelfHealing(**kwargs)
		else:
			print "Usage error: Invalid optins"
                	usage()
        else:
		usage()
if __name__ == '__main__':
	main()
	print "end"
