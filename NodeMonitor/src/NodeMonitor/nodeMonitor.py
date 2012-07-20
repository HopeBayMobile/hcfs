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

'''
Notification Data Format.
{
    component_name:[component name],
    event=[event name],
    level:[level name],
    data:{[performance_data]},
    time:[date time string],
    message:[reserve]
}

Exception Data Format.
{
    component_name:[component name],
    message:[message],
    time:[date time]
}
'''

def post_data(url, data):
    data = json.dumps(data)
    req = urllib2.Request(url, data, {'Content-Type': 'application/json'})
        
    #TODO: time out mechanism
    response = None
    f = urllib2.urlopen(req)
    response = f.read()
    f.close
    return response


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
        ret = "WARNING"
        for info in disk_info:
            if not info["healthy"]:
                ret = "ERROR"

        return ret

    def send_disk_event(self):
        """
        send event to the receiver 
        """
        logger = util.getLogger(name="DiskChecker")
        event2post = None
        try:
            disk_info = self.check_all_disks()
            event = {
                "component_name": self.component_name,
                "event": self.event_name,
                "level": self.decide_event_level(disk_info),
                "data": json.dumps(disk_info),
                "time": str(datetime.now()),
                "message": "",
            }
            event2post = json.dumps(event)

        except Exception as e:
            logger.error(str(e))
            event = {
                "component_name": self.component_name,
                "message": str(e),
                "time": str(datetime.now()),
            }
            event2post = json.dumps(event)

        logger.info(event2post)
        post_data(self.receiverUrl, event2post)

        
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

    def post_data(self, url, data):
        data = json.dumps(data)
        req = urllib2.Request(url, data, {'Content-Type': 'application/json'})
        
        #TODO: time out mechanism
        response = None
        f = urllib2.urlopen(req)
        response = f.read()
        f.close
        return response

    def run(self):
        logger = util.getLogger(name="NodeMonitor.run")
        logger.info("start")

        try:
            while True:
                self.DC.send_disk_event()
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
