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

TIMEOUT = 180

class SwiftEventMgr(Daemon):
	def __init__(self, pidfile):
		Daemon.__init__(self, pidfile)

		self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
		self.port = self.masterCfg.getKwparams()["eventMgrPort"]

	def subscribe(self):
		pass

	def unSubscribe(self):
		pass

	def __recvData(self, conn):
		'''
		assume a socket disconnect (data returned is empty string) 
                means all data was #done being sent.
		'''
   		total_data=[]
   		while True:
       			data = conn.recv(8192)
       			if not data: 
				break
       			total_data.append(data)
   		return ''.join(total_data)

	def __handleEvent(self, eventStr):
		logger = util.getLogger(name="SwiftEventMgr.__handleEvent")
		logger.info("start")
		event = None
		try:
			if not self.isValidEventStr(eventStr):
				logger.error("Incomplete read of data from %s"%(str(address)))
				return

			#Add your code here
			#event = json.loads(jsonStr)
		finally:
			logger.info("end")		

		
	def isValidEventStr(self, eventStr):
		'''
		Check if eventStr is a valid json str representing an event
		'''	
		#Add your code here
		return True

	def __doJob(self, sock):
   		while True:
			data = None
			event = None
			try:
				conn, address=sock.accept()
				conn.settimeout(TIMEOUT)
				data = self.__recvData(conn)
				conn.close()

			except socket.timeout:
				logger.error("Timeout to receive data from %s"%(str(address)))
			except socket.error as e:
				logger.error(str(e))
			except Exception as e:
				logger.error(str(e))
				sys.exit(1)

			if data:
				self.__handleEvent(data)

	def run(self):
		logger = util.getLogger(name="SwiftEventMgr.run")
		logger.info("%s"%self.port)

		sock = None
		try:
   			sock=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
   			sock.bind(('',int(self.port)))
   			sock.listen(5)
   			logger.info('started on %s'%self.port)
		except socket.error as e:
			msg = "Failed to bind port %s for %s"%(self.port, str(e))
			logger.error(msg)
			sys.exit(1)

		self.__doJob(sock)



if __name__ == "__main__":
	daemon = SwiftEventMgr('/var/run/SwiftEventMgr.pid')
	if len(sys.argv) == 2:
		if 'start' == sys.argv[1]:
			daemon.start()
		elif 'stop' == sys.argv[1]:
			daemon.stop()
		elif 'restart' == sys.argv[1]:
			daemon.restart()
		else:
			print "Unknown command"
			sys.exit(2)
		sys.exit(0)
	else:
		print "usage: %s start|stop|restart" % sys.argv[0]
		sys.exit(2)

