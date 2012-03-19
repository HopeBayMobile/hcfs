'''
Created on 2011/11/21

@authors: Rice, CW and Ken
'''

import sys
import os
import socket
import posixfile
import json
import subprocess
import re
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser



class ParsePeerStatus:
	def __init__(self):
		pass
	
	def __executePopen(self, cmd):
        	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        	po.wait()
                return po.returncode, po.stdout.read()
	def getPeerStatus(self, hostname):
		cmd = "sudo gluster peer status"
		(ret, output) = self.__executePopen(cmd)
		if ret !=0:
			print >>sys.stderr, output
			return ""
		pattern = "Hostname: %s\nUuid: (\w|-)+\nState: (\w| )+ \(\w+\)"%hostname 
		m =re.search(pattern, output)
		if m is None:
			return ""
		state = m.group(0).split('\n')[2]
		return state

	def isConnected(self, hostname):
		cmd = "sudo gluster peer status"
                (ret, output) = self.__executePopen(cmd)
                if ret !=0:
                        print >>sys.stderr, output
                        return False
                pattern = "Hostname: %s\nUuid: (\w|-)+\nState: (\w| )+ \(Connected\)"%hostname
                m =re.search(pattern, output)
                if m is None:
                        return False
                return True


if __name__ == '__main__':
	
	PPS = ParsePeerStatus()
	PPS.getPeerStatus("ntu06")
	print PPS.isConnected("ntu06")
