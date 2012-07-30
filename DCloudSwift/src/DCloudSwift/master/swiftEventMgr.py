import os
import sys
import time
import socket
import random
import pickle
import signal
import json
import sqlite3
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
from util.database import MaintenanceBacklogDatabaseBroker

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
            healthyDisks = [disk["SN"] for disk in data if disk["healthy"] and disk["SN"]]
            brokenDisks = [disk["SN"] for disk in data if not disk["healthy"] and disk["SN"]]
            timestamp = event["time"]
        except Exception as e:
            logger.error(str(e))
            return False

        if not isinstance(event["hostname"], str) and not isinstance(event["hostname"], unicode):
            logger.error("Wrong type of hostname")
            return False
        if not isinstance(event["time"], int):
            logger.error("Wrong type of time")
            return False
        if SwiftEventMgr.getExpectedDiskCount(event["hostname"]) is None:
            logger.error("Failed to get expected disk count of %s" % event["hostname"])
            return False

        return True


    @staticmethod
    def updateDiskInfo(event, nodeInfoDbPath=GlobalVar.NODE_DB):
        '''
        update disk info according to the disk event

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

        hostname = event["hostname"]
        data = json.loads(event["data"])
        node = nodeInfoDb.get_info(hostname)
        if not node:
            logger.error("%s is not registered!!" % hostname)
            return None

        #TODO: handle invalid old_disk_info
        try:
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
        new_disk_info = SwiftEventMgr.updateDiskInfo(event)

    '''
    Heartbeat format
    {
        event: "heartbeat",
        nodes:
        [
            {
                hostname:<hostname>,
                role:<enum:MH,MA,MD,MMS>,
                status:<enum:alive,unknown,dead>
            },...
        ]
    }
    '''
    @staticmethod
    def isValidHeartbeat(event):
        logger = util.getLogger(name="SwiftEventMgr.isValidHeartbeat")
        try:
            for node in event["nodes"]:
                hostname = node["hostname"]
                role = node["role"]
                status = node["status"]
                if not isinstance(hostname, str) and not isinstance(hostname, unicode):
                    logger.error("Wrong type of hostname!")
                    return False
                if not isinstance(role, str) and not isinstance(role, unicode):
                    logger.error("Wrong type of role!")
                    return False
                if not isinstance(status, str) and not isinstance(status, unicode):
                    logger.error("Wrong type of status!")
                    return False
        except Exception as e:
            logger.error(str(e))
            return False

        return True

    @staticmethod
    def updateNodeStatus(node, nodeInfoDbPath):
        '''
        update node status according to the heartbeat event
        '''
        logger = util.getLogger(name="SwiftEventMgr.updateNodeStatus")

        try:
                hostname = node["hostname"]
                status = node["status"]
                timestamp = int(time.time())
                nodeInfoDb = NodeInfoDatabaseBroker(nodeInfoDbPath)
                row = nodeInfoDb.update_node_status(hostname=hostname, status=status, timestamp=timestamp)
                return row

        except Exception as e:
            logger.error(str(e))
            return None

    @staticmethod
    def handleHeartbeat(event, nodeInfoDbPath=GlobalVar.NODE_DB):
        logger = util.getLogger(name="swifteventmgr.handleHeartbeat")
        
        if not SwiftEventMgr.isValidHeartbeat(event):
            logger.error("Invalid heartbeat event")
            return None
        
        nodes = event["nodes"]
        for node in nodes:
            newNodeStatus = SwiftEventMgr.updateNodeStatus(node, nodeInfoDbPath)
            return newNodeStatus

    @staticmethod
    def handleEvents(notification):
        logger = util.getLogger(name="swifteventmgr.handleEvents")
        logger.info("%s" % notification)
        event = None
        eventName = None
        try:
            event = json.loads(notification)
        except:
            logger.error("Notification %s is not a legal json string" % notification)
            return 1

        try:
           eventName = event["event"].lower()
        except Exception as e:
           logger.error("Failed to get event name for %s" % str(e))
           return 1

        #Add your code here
        if eventName == "hdd":
            SwiftEventMgr.handleHDD(event)
        elif eventName == "heartbeat":
            SwiftEventMgr.handleHeartbeat(event)
            
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
            threadPool = reactor.getThreadPool()
            threadPool.adjustPoolsize(minthreads=1, maxthreads=1)
            reactor.listenTCP(int(self.port), factory)
            reactor.run()
        except twisted.internet.error.CannotListenError as e:
            logger.error(str(e))


def getSection(inputFile, section):
    ret = []
    with open(inputFile) as fh:
        lines = fh.readlines()
        start = 0
        for i in range(len(lines)):
            line = lines[i].strip()
            if line.startswith('[') and section in line:
                start = i + 1
                break
        end = len(lines)
        for i in range(start, len(lines)):
            line = lines[i].strip()
            if line.startswith('['):
                end = i
                break

        for line in lines[start:end]:
            line = line.strip()
            if len(line) > 0:
                ret.append(line)

        return ret


def parseNodeListSection(inputFile):
    lines = getSection(inputFile, "nodeList")
    try:
        nodeList = []
        nameSet = set()
        for line in lines:
            line = line.strip()
            if len(line) > 0:
                tokens = line.split()
                if len(tokens) != 1:
                    raise Exception("[nodeList] contains an invalid line %s" % line)

                name = tokens[0]
                if name in nameSet:
                    raise Exception("[nodeList] contains duplicate names")

                nodeList.append({"hostname": name})
                nameSet.add(name)

        return nodeList
    except IOError as e:
        msg = "Failed to access input files for %s" % str(e)
        raise Exception(msg)


def initializeNodeInfo():
    '''
    Command line implementation of node info initialization.
    '''

    ret = 1

    Usage = '''
    Usage:
        dcloud_initialize_node_info
    arguments:
        None
    '''

    if (len(sys.argv) != 1):
        print >> sys.stderr, Usage
        sys.exit(1)

    inputFile = "/etc/delta/inputFile"
    nodeList = parseNodeListSection(inputFile)

    try:
        nodeInfoDb = NodeInfoDatabaseBroker(GlobalVar.NODE_DB)
        nodeInfoDb.initialize()
    except sqlite3.OperationalError as e:
        print >> sys.stderr, "Node info already exists!!"
        sys.exit(1)

    for node in nodeList:
        hostname = node["hostname"]
        status = "alive"
        timestamp = int(time.time())

        disk_info = {
                        "timestamp": timestamp,
                        "missing": {"count": 0, "timestamp": timestamp},
                        "broken": [],
                        "healthy": [],
        }
        disk = json.dumps(disk_info)

        mode = "service"
        switchpoint = timestamp  

        nodeInfoDb.add_node(hostname=hostname, 
                            status=status, 
                            timestamp=timestamp, 
                            disk=disk, 
                            mode=mode, 
                            switchpoint=switchpoint)

def initializeMaintenanceBacklog():
    '''
    Command line implementation of maintenance backlog initialization.
    '''

    ret = 1

    Usage = '''
    Usage:
        dcloud_initialize_backlog
    arguments:
        None
    '''

    if (len(sys.argv) != 1):
        print >> sys.stderr, Usage
        sys.exit(1)

    try:
        backlog = MaintenanceBacklogDatabaseBroker(GlobalVar.MAINTENANCE_BACKLOG)
        backlog.initialize()
    except sqlite3.OperationalError as e:
        print >> sys.stderr, "Maintenance backlog already exists!!"
        sys.exit(1)

def clearNodeInfo():
    '''
    Command line implementation of node info clear
    '''

    ret = 1

    Usage = '''
    Usage:
        dcloud_clear_node_info
    arguments:
        None
    '''

    if (len(sys.argv) != 1):
        print >> sys.stderr, Usage
        sys.exit(1)

    if os.path.exists(GlobalVar.NODE_DB):
        os.system("rm %s" % GlobalVar.NODE_DB)
        ret = 0

    return ret


def clearMaintenanceBacklog():
    '''
    Command line implementation of clearing maintenance backlog
    '''

    ret = 1

    Usage = '''
    Usage:
        dcloud_clear_backlog
    arguments:
        None
    '''

    if (len(sys.argv) != 1):
        print >> sys.stderr, Usage
        sys.exit(1)

    if os.path.exists(GlobalVar.MAINTENANCE_BACKLOG):
        os.system("rm %s" % GlobalVar.MAINTENANCE_BACKLOG)
        ret = 0

    return ret


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
#        clearNodeInfo()
#        clearMaintenanceBacklog()
#        initializeMaintenanceBacklog()    
#
#        nodeInfoDb = NodeInfoDatabaseBroker(GlobalVar.NODE_DB)
#        rows = nodeInfoDb.show_node_info_table()
#        for row in rows:
#             print row
        print "Unknown command"
        print "usage: %s start|stop|restart" % sys.argv[0]
        sys.exit(2)
