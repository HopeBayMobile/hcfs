import os, sys, time
import socket
import random
import pickle
import signal


WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/"%BASEDIR)

from util.SwiftCfg import SwiftMasterCfg
from util.daemon import Daemon
from util.util import GlobalVar
from util import util


class SwiftEventMgr(Daemon):
	def __init__(self, pidfile):
		Daemon.__init__(self, pidfile)

		self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
		self.port = self.masterCfg.getKwparams()["eventMgrPort"]

	def doJob(self):
		logger = util.getLogger(name="SwiftEventMgr.doJob")
		logger.info("start")
		

	def run(self):
		logger = util.getLogger(name="SwiftEventMgr.run")
		logger.info("hello")
		logger.info("%s"%self.port)

		while True:
			logger.info("%s"%self.port)
			time.sleep(10)


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
