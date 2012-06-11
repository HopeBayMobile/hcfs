import os, sys, time
import socket
import random
import pickle
import signal
import json


WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/"%BASEDIR)

from util.SwiftCfg import SwiftMasterCfg
from util.daemon import Daemon
from util.util import GlobalVar
from util import util

import socket,struct,sys,time

TIMEOUT = 300
socket.setdefaulttimeout(TIMEOUT)


if __name__ == "__main__":
	#daemon = SwiftEventMgr('/var/run/SwiftEventMgr.pid')
	#if len(sys.argv) == 2:
	#	if 'start' == sys.argv[1]:
	#		daemon.start()
	#	elif 'stop' == sys.argv[1]:
	#		daemon.stop()
	#	elif 'restart' == sys.argv[1]:
	#		daemon.restart()
	#	else:
	#		print "Unknown command"
	#		sys.exit(2)
	#	sys.exit(0)
	#else:
	#	print "usage: %s start|stop|restart" % sys.argv[0]
	#	sys.exit(2)

   	#start_server(recv_type='end')


	def send_end(data):
   		sock=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
   		sock.connect(('localhost',5308))
   		sock.sendall(data)
		time.sleep(10)
   		sock.close()
		sock.close()
		sock.close()

	send_end("Hello")
	
