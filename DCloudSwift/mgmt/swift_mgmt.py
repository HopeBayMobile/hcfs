# Created on 2012/04/17 by CW
# Management functions invoked by task_worker of PDCM

import sys
import os
import socket
import posixfile
import time
import json
import subprocess
import shlex
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

sys.path.append('/DCloudSwift/master')
from SwiftDeploy import proxyDeploy, storageDeploy, addStorage, rmStorage 

#import task_worker from PDCM
sys.path.append('/var/www/PDCM/task/worker')
from task_worker import WorkerTask, task_class, task


@task_class
class DeploySwift(WorkerTask):
        def __init__(self):
                WorkerTask.__init__(self)

        def run(self, params):
                kwparams = json.loads(params)
		mgmt = SwiftDeploy(kwparams['proxyList'], kwparams['storageList'])
		mgmt.proxyDeploy()
		mgmt.storageDeploy()
                #return self.__mgmt.createVolume(**kwparams)


@task_class
class AddStorage(WorkerTask):
        def __init__(self):
                WorkerTask.__init__(self)

        def run(self, params):
                kwparams = json.loads(params)
		mgmt = SwiftDeploy(kwparams['proxyList'], kwparams['storageList'])
		mgmt.addStorage()
                #return self.__mgt.replaceServer(**kwparams)


@task_class
class RmStorage(WorkerTask):
        def __init__(self):
                WorkerTask.__init__(self)

        def run(self,params):
		kwparams = json.loads(params)
		mgmt = SwiftDeploy(kwparams['proxyList'], kwparams['storageList'])
		mgmt.rmStorage()
                #return self.__mgt.triggerSelfHealing(**kwparams)


if __name__ == '__main__':
	ds = DeploySwift()
	print "==========GUI DeploySwift==========\n"
        param1 = {'receiver': 'ntu01',
                  'hostList': ['ntu01', 'ntu02', 'ntu03'],
                  'volType': 'distribute',
                  'count': '10'
        }
        jsonStr1 = json.dumps(param1, sort_keys=True, indent=4)
        ds.run(jsonStr1)

