import os
import sys
import time
import socket
import random
import pickle
import signal
import json
import urllib2
from datetime import datetime

from util.daemon import Daemon
from util.util import GlobalVar
from util import util
from components.disk_info import DiskInfo
from components.mem_info import MemInfo
from components.net_info import NetInfo
from components.cpu_info import CpuInfo
from components.daemon_info import DaemonInfo
from components.os_info import OSInfo

timeout = 60
socket.setdefaulttimeout(timeout)
random.seed(os.urandom(100))

'''
Notification Data Format.
{
    component_name:[component name],
    event=[event name],
    level:[level name],
    data:{[performance_data]},
    time:[integer],
    message:[reserve]
}

Exception Data Format.
{
    component_name:[component name],
    message:[message],
    time: [integer]
}
'''

'''
Heartbeat Publish Format
{
    sn: <integer>,
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

Event Publish Format
{
    sn: <integer>
    compenent_name: " sensor component name",
    event: "event name",
    level: "level name",
    hostname: "node's hostname",
    data: "performance data",
    time: "trigger time",
}
'''

def post_data(url, data):
    req = urllib2.Request(url, data, {'Content-Type': 'application/json'})
        
    response = None
    f = urllib2.urlopen(req)
    response = f.read()
    f.close

    return response

class RuntimeInfo:
    def __init__(self, receiverUrl):
        self.MI = MemInfo()
        self.NI = NetInfo()
        self.CI = CpuInfo()
        self.DI = DaemonInfo()
        self.OI = OSInfo()
        self.receiverUrl = receiverUrl
        self.component_name = "RUNTIME_INFO"
        self.event_name = "RUNTIME_INFO"

    def collect_info(self):
        """
        collect runtime info

	@rtype: {
                    cpu: {"usage" <int: percentage>},

                    mem:{ 
                       "total": total number of memory in bytes 
                       "usage": percentage of used memory
                    },

                    net: {
                           "eth0": { "receive": bits trnasmitted per second (integer),
                                     "transmit" bits transmitted per second (interger)},
                           "eth1": { ... },
                       ...
                    },

                    daemon: {"daemon_name1": "on", "daemon_name2": "off", ...},
					os: {
					    "description": "Ubuntu12.04.1LTS"

					},

        }
  
        @return: stats of the node
        """
        ret = {}
        ret["cpu"] = self.CI.check_cpu()
        ret["net"] = self.NI.check_network()
        ret["mem"] = self.MI.check_memory()
        ret["daemon"] = self.DI.check_daemons()
        ret["os"] = {"description": self.OI.get_distrib_description()}
        return ret


    def send_runtime_info_event(self):
        """
        send event to the receiver 
        """
        logger = util.getLogger(name="RuntimeInfo")
        eventEncoding = None
        try:
            info = self.collect_info()
            event = {
                "hostname": socket.gethostname(),
                "component_name": self.component_name,
                "event": self.event_name,
                "level": "INFO",
                "data": json.dumps(info),
                "time": int(time.time()),
            }
            eventEncoding= json.dumps(event)
            logger.info(eventEncoding)

        except Exception as e:
            logger.error(str(e))
            event = {
                "component_name": self.component_name,
                "message": str(e),
                "time": int(time.time()),
            }
            logger.error(json.dumps(event))

        if eventEncoding:
            post_data(self.receiverUrl, eventEncoding)


class DiskChecker:
    def __init__(self, receiverUrl):
        self.DI = DiskInfo()
        self.receiverUrl = receiverUrl
        self.component_name = "DISK_INFO"
        self.event_name = "HDD"

    def check_all_disks(self):
        """
        check status of all detected disks

	@rtype: [
                    {
                       "SN": "serial number of the disk", 
                       "healthy": bool, 
                       "capacity": capacity of the disk in bytes, 
                       "usage": used percentage of the disk
                    }, ...
        ]
  
        @return: info of all detected disks
        """
        ret = self.DI.check_all_disks()
        return ret

    def decide_event_level(self, disk_info):
        ret = "INFO"
        for info in disk_info:
            if not info["healthy"]:
                ret = "ERROR"

        return ret

    def send_disk_event(self):
        """
        send event to the receiver 
        """
        logger = util.getLogger(name="DiskChecker")
        eventEncoding = None
        try:
            disk_info = self.check_all_disks()
            event = {
                "hostname": socket.gethostname(),
                "component_name": self.component_name,
                "event": self.event_name,
                "level": self.decide_event_level(disk_info),
                "data": json.dumps(disk_info),
                "time": int(time.time()),
            }
            eventEncoding= json.dumps(event)
            logger.info(eventEncoding)

        except Exception as e:
            logger.error(str(e))
            event = {
                "component_name": self.component_name,
                "message": str(e),
                "time": int(time.time()),
            }
            logger.error(json.dumps(event))

        if eventEncoding:
            post_data(self.receiverUrl, eventEncoding)

class Heartbeat:
    def __init__(self, receiverurl):
        self.receiverurl = receiverurl

    def send_heartbeat(self):
        """
        send heartbeat to the receiver 
        """
        logger = util.getLogger(name="heartbeat")
        heartbeatencoding = None
        try:
    
            heartbeat = {
                "event": "heartbeat",
                "nodes": [
                             {
                               "hostname": socket.gethostname(),
                               "role": "mh",
                               "status": "alive",
                             },
                         ]
            }

            heartbeatencoding = json.dumps(heartbeat)
            logger.info(heartbeatencoding)

        except exception as e:
            logger.error(str(e))
            event = {
                "component_name": "heartbeat",
                "message": str(e),
                "time": int(time.time()),
            }
            logger.error(json.dumps(event))

        if heartbeatencoding:
            post_data(self.receiverurl, heartbeatencoding)
        

class NodeMonitor(Daemon):
    def __init__(self, pidfile):
        Daemon.__init__(self, pidfile)
        self.receiverUrl = util.getReceiverUrl()
        try:
            
            self.sensorInterval = util.getSensorInterval()
            if self.sensorInterval[0] > self.sensorInterval[1]:
                raise
            if self.sensorInterval[0] <= 0:
                raise
        except:
            self.sensorInterval = (30, 60)

        self.DC = DiskChecker(self.receiverUrl)
        self.HB = Heartbeat(self.receiverUrl)
        self.RI = RuntimeInfo(self.receiverUrl)

    def run(self):
        logger = util.getLogger(name="NodeMonitor.run")
        logger.info("start")

        while True:
            try:
                try: 
                    self.HB.send_heartbeat()
                except Exception as e:
                    logger.error(str(e))
                 
                try:
                    self.DC.send_disk_event()
                except Exception as e:
                    logger.error(str(e))

                try:
                    self.RI.send_runtime_info_event()
                except Exception as e:
                    logger.error(str(e))

                window = random.randint(*self.sensorInterval)
                time.sleep(window)

            except Exception as e:
                logger.error(str(e))

if __name__ == "__main__":
    daemon = NodeMonitor('/var/run/NodeMonitor.pid')
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
