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

from util.SwiftCfg import SwiftMasterCfg
from util.daemon import Daemon
from util.util import GlobalVar
from util import util

class SwiftMaintainer:
    def __init__(self, api_server):
        # TODO: error checking
        self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
        self.port = self.masterCfg.getKwparams()["eventMgrPort"]
        self.page = self.masterCfg.getKwparams()["eventMgrPage"]
        self.ip = util.getIpAddress()
        self.api_server = api_server

    def post_data(self, url, data):
        data = json.dumps(data)
        req = urllib2.Request(url, data, {'Content-Type': 'application/json'})
        
        #TODO: time out mechanism
        response = None
        f = urllib2.urlopen(req)
        response = f.read()
        f.close
        return response

    def subscribe(self, event, level_name='warning', interval=None, hostname_list=[]):
        ret = {"code": 1, "message": ""}

        url = "http://"+self.api_server+"/subscribeEvent"
        data = {
                'subscriber_name': 'storage_team', 
                'ipv4': self.ip,
                'port': int(self.port), 
                'event_callback': self.page, 
                'event': event, 
                'interval': interval, 
                'level_name': level_name, 
                'hostname': hostname_list,
        }

        try:
            response = self.post_data(url=url, data=data)
        except Exception as e:
            ret["message"] = str(e)
            return ret

        try:
            response = json.loads(response)
            if response["result"] != "fail":
                ret["code"] = 0
            else:
                ret["message"] = response["message"]
        except:
            ret["message"] = "The response is not in legal format"

        return ret

    def unsubscribe(self):
        pass


if __name__ == "__main__":
    #SM = SwiftMaintainer("172.16.229.220:8080")
    #data = {}
    #response = SM.post_data("http://172.16.229.220:8080/subscribeEvent", data)
    #response = SM.post_data("http://172.16.229.63:5308/events", data)
    #response = SM.subscribe(event='cpu temperature')
    #print response
    print swiftEvents.HDD.healthy.YES
