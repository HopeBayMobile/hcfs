import os, sys, time
import socket
import random
import pickle
import signal
import json
from twisted.web.server import Site
from twisted.web.resource import Resource
from twisted.internet import reactor


WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/"%BASEDIR)

from util.SwiftCfg import SwiftMasterCfg
from util.daemon import Daemon
from util.util import GlobalVar
from util import util


TIMEOUT = 180
EVENTS_FIELD= 'events'


class SwiftEventMgr(Daemon):
	def __init__(self, pidfile):
		Daemon.__init__(self, pidfile)

		self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
		self.port = self.masterCfg.getKwparams()["eventMgrPort"]

	def subscribe(self):
		pass

	def unSubscribe(self):
		pass
		
	def isValidEventStr(self, eventStr):
		'''
		Check if eventStr is a valid json str representing an event
		'''	
		#Add your code here
		return True

	@staticmethod
	def handleEvents(jsonStr4Events):
		logger = util.getLogger(name="swifteventmgr.handleEvents")
		logger.info("%s"%jsonStr4Events)
		#Add your code here
		
	class EventsPage(Resource):
    		def render_GET(self, request):
        		return '<html><body><form method="POST"><input name="%s" type="text" /></form></body></html>'%EVENTS_FIELD

    		def render_POST(self, request):
			reactor.callLater(1, SwiftEventMgr.handleEvents, request.args[EVENTS_FIELD][0])
        		return '<html><body>Thank you!</body></html>'

	def run(self):
		logger = util.getLogger(name="SwiftEventMgr.run")
		logger.info("%s"%self.port)

		root = Resource()
		root.putChild("events", SwiftEventMgr.EventsPage())
		factory = Site(root)
		reactor.listenTCP(int(self.port), factory)
		reactor.run()


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
	
