import os
import sys
import time
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
sys.path.append("%s/DCloudSwift/" % BASEDIR)

from util.SwiftCfg import SwiftMasterCfg
from util.daemon import Daemon
from util.util import GlobalVar
from util import util

class SwiftEventMgr(Daemon):
    def __init__(self, pidfile):
        Daemon.__init__(self, pidfile)

        logger = util.getLogger(name="swifteventmgr.handleEvents")
        self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
        self.port = self.masterCfg.getKwparams()["eventMgrPort"]
        self.page = self.masterCfg.getKwparams()["eventMgrPage"]

    def isValidNotification(self, notification):
        '''
        Check if notification is a valid json str representing an event list
        '''
        #Add your code here
        return True

    @staticmethod
    def handleEvents(notification):
        logger = util.getLogger(name="swifteventmgr.handleEvents")
        logger.info("%s" % notification)
        #Add your code here

        time.sleep(10)

    class EventsPage(Resource):
            def render_GET(self, request):
                # return '<html><body><form method="POST"><input name=%s type="text" /></form></body></html>' % FROM_MONITOR
                return '<html><body>I am the swift event manager!!</body></html>'

            def render_POST(self, request):
                logger = util.getLogger(name="swifteventmgr.render_POST")
                # body=request.args['body'][0]
                # reactor.callLater(0.1, SwiftEventMgr.handleEvents, request.content.getvalue())
                # d = deferLater(reactor, 0.1, SwiftEventMgr.handleEvents, request.content.getvalue())
                # d.addCallback(printResult)
                from twisted.internet import threads
                try:
                    d = threads.deferToThread(SwiftEventMgr.handleEvents, request.content.getvalue())
                except Exception as e:
                    logger.error("%s" % str(e))

                return '<html><body>Got it!!</body></html>'

    def run(self):
        logger = util.getLogger(name="SwiftEventMgr.run")
        logger.info("%s" % self.port)

        root = Resource()
        root.putChild(self.page, SwiftEventMgr.EventsPage())
        factory = Site(root)

        try:
            reactor.listenTCP(int(self.port), factory)
            reactor.run()
        except twisted.internet.error.CannotListenError as e:
            logger(str(e))


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
