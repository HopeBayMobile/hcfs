import os
import sys
import time
import socket
import random
import pickle
import signal
import json
import urllib2

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/" % BASEDIR)

from util.daemon import Daemon
from util.util import GlobalVar
from util import util

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
                window = random.randint(*self.sensorInterval)
                logger.info("%d" % window)
                time.sleep(window)

        except Exception as e:
            logger(str(e))


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
