import os
import sys
import time
import socket
import random
import pickle
import signal
import json
from ConfigParser import ConfigParser
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
from util.database import NodeInfoDatabaseBroker

class SwiftEventMgr(Daemon):
    def __init__(self, pidfile):
        Daemon.__init__(self, pidfile)

        logger = util.getLogger(name="swifteventmgr.handleEvents")
        self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
        self.port = self.masterCfg.getKwparams()["eventMgrPort"]
        self.page = self.masterCfg.getKwparams()["eventMgrPage"]

    @staticmethod
    def getExpectedDiskCount(hostname):
        '''
        Get expected disk count of a node

        @type  hostname: string
        @param hostname: hostname of the node to query expected disk count
        @rtype: integer
        @return: expected disk count of the queried node. Return None in case of exception.
        '''
        config = ConfigParser()

        try:
            with open(GlobalVar.ORI_SWIFTCONF, "rb") as fh:
                config.readfp(fh)

            return int(config.get('storage', 'deviceCnt'))
        except:
            logger.error("Failed to read deviceCnt from %s" %  GlobalVar.ORI_SWIFTCONF)
            return None


    '''
    HDD event format
    {
        component_name: "disk_info",
        event: "HDD",
        level: INFO|WARNING|ERROR,
        hostname: <hostname>
        data: [
                  { 
                     SN: <serial number>,
                     healthy: True|False
                  }, 

                  {
                      SN: <string: serial number>,
                      healthy: True|False
                  }, ... 
              ],
        time: <integer>
    }
    '''

    @staticmethod
    def isValidDiskEvent(event):
        logger = util.getLogger(name="SwiftEventMgr.isValidDiskEvent")
        try:
            hostname = event["hostname"]
            data = json.loads(event["data"])
            expectedDiskCount = SwiftEventMgr.getExpectedDiskCount(event["hostname"])
            healthyDisks = [disk["SN"] for disk in data if disk["healthy"] and disk["SN"]]
            brokenDisks = [disk["SN"] for disk in data if not disk["healthy"] and disk["SN"]]
            timestamp = event["time"]
        except Exception as e:
            logger.error(str(e))
            return False

        if not isinstance(hostname, str) or not isinstance(timestamp, int):
            return False

        if expectedDiskCount is None:
            return False

        return True


    @staticmethod
    def updateDiskInfo(event, nodeInfoDbPath=GlobalVar.NODE_DB):
        '''
        update disk info according to the event

        @type  event: dictioary
        @param event: disk event
        @rtype: dictionary
        @return: updated disk info
        '''
        logger = util.getLogger(name="SwiftEventMgr.updateDiskInfo")
        new_disk_info = {
                            "timestamp": 0,
                            "missing": {"count": 0, "timestamp": 0},
                            "broken": [],
                            "healthy": [],
        }

        nodeInfoDb = NodeInfoDatabaseBroker(nodeInfoDbPath)

        if not SwiftEventMgr.isValidDiskEvent(event):
            logger.error("Invalid disk event!!")
            return None

        # TODO: handle the exception of invalid old_disk_info
        try:
            hostname = event["hostname"]
            data = json.loads(event["data"])
            old_disk_info = json.loads(nodeInfoDb.get_info(hostname)["disk"])
            expectedDiskCount = SwiftEventMgr.getExpectedDiskCount(hostname)

            detectedDiskSNs = {disk["SN"] for disk in data if disk["SN"]}
            knownDiskSNs = {disk["SN"] for disk in old_disk_info["broken"]+old_disk_info["healthy"] if disk["SN"]}

            if old_disk_info["timestamp"] >= event["time"]:
                logger.warn("Old disk events are received")
                return None
            else:
                new_disk_info["timestamp"] = event["time"]

            # Update missing disks info
            new_disk_info["missing"]["count"] = max(expectedDiskCount - len(detectedDiskSNs), 0)
            if len( knownDiskSNs-detectedDiskSNs) > 0 and len(detectedDiskSNs) < expectedDiskCount:
                new_disk_info["missing"]["timestamp"] = event["time"]
            else:
                new_disk_info["missing"]["timestamp"] = old_disk_info["missing"]["timestamp"]
            
            # Update healthy disks info
            detectedHealthyDisks = [disk for disk in data if disk["healthy"]]
            new_disk_info["healthy"] = [{"SN": disk["SN"], "timestamp": event["time"]} for disk in detectedHealthyDisks]

            # Update broken disks info
            detectedBrokenDisks = [disk for disk in data if not disk["healthy"]]
            detectedBrokenDiskSNs = [disk["SN"] for disk in data if not disk["healthy"]]
            knownBrokenDiskSNs = {disk["SN"] for disk in old_disk_info["broken"] if disk["SN"]}

            new_disk_info["broken"] = [{"SN": disk["SN"], "timestamp": event["time"]} for disk in detectedBrokenDisks
                                        if not disk["SN"] in knownBrokenDiskSNs and disk["SN"]]

            new_disk_info["broken"] += [{"SN": disk["SN"], "timestamp": disk["timestamp"]} for disk in old_disk_info["broken"]
                                        if disk["SN"] in detectedBrokenDiskSNs and disk["SN"]]

        except Exception as e:
            logger.error(str(e))
            return None 

        try:
            disk = json.dumps(new_disk_info)
            nodeInfoDb.update_node_disk(event["hostname"], disk)
        except Exception as e:
            logger.error("Failed to update database for %s" % str(e))
            return None

        return new_disk_info


    @staticmethod
    def handleHDD(event):
        logger = util.getLogger(name="swifteventmgr.handleHDD")
        new_disk_info = SwiftEventMgr.extractDiskInfo(event)

    @staticmethod
    def handleEvents(notification):
        logger = util.getLogger(name="swifteventmgr.handleEvents")
        logger.info("%s" % notification)
        event = None
        try:
            event = json.loads(notification)
        except:
            logger.error("Notification %s is not a legal json string" % notification)

        #Add your code here
        time.sleep(10)

    class EventsPage(Resource):
            def render_GET(self, request):
                return '<html><body>I am the swift event manager!!</body></html>'

            def render_POST(self, request):
                logger = util.getLogger(name="swifteventmgr.render_POST")
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
            sys.exit(2)
    else:
        fakedb = "/etc/test.db"
        os.system("rm /etc/test.db")
        print SwiftEventMgr.updateDiskInfo({}, fakedb)

        print "Unknown command"
        print "usage: %s start|stop|restart" % sys.argv[0]
        sys.exit(2)
