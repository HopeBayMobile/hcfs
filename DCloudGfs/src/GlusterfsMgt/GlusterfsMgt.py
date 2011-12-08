'''
Created on 2011/11/19

@authors: Rice, CW and Ken

Modified by CW on 2011/11/24
Modified by CW on 2011/11/25
Modified by CW on 2011/11/28
Modified by CW on 2011/11/29
'''

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
import uuid

#import from Delta Cloud Manager
#from task_worker import WorkerTask
from test import WorkerTask

#Third party packages
import paramiko
import pexpect


class volumeCreate(WorkerTask):
	def __init__(self, task_id):
		WorkerTask.__init__(self, task_id)
		self.__taskId = task_id
		self.__mgt = GlusterfsMgt(self)

	def run(self, params):
		kwparams = json.loads(params)
		return self.__mgt.createVolume(**kwparams)


class replaceServer(WorkerTask):
	def __init__(self, task_id):
		WorkerTask.__init__(self, task_id)
		self.__taskId = task_id
		self.__mgt = GlusterfsMgt(self)

	def run(self, params):
		kwparams = json.loads(params)
		return self.__mgt.replaceServer(**kwparams)


class triggerSelfHealing(WorkerTask):
	def __init__(self, task_id):
		WorkerTask.__init__(self, task_id)
		self.__taskId = task_id
		self.__mgt = GlusterfsMgt(self)

	def run(self,params):
		kwparams = json.loads(params)
		return self.__mgt.triggerSelfHealing(**kwparams)


class GlusterfsMgt:
	def __init__(self, worker):
		
		config = ConfigParser()
		config.readfp(open("../Global.ini"))
		self.__codeDir = config.get('glusterfs', 'codeDir')
		self.__worker = worker

		config2 = ConfigParser()
		config2.readfp(open("../DCloudGfs.ini"))
		self.__username = config2.get('main', 'username')
		self.__password = config2.get('main', 'password')
		self.__taskReceiver = {}

	def __setupSshConn(self, receiver):
                if self.__username != "":
                        cmd = "ssh -o StrictHostKeyChecking=no -l " + self.__username + " " + receiver
                else:
                        cmd = "ssh -o StrictHostKeyChecking=no " + receiver
                p=pexpect.spawn(cmd)

		while True:
                        i = p.expect(['login', 'password', 'Permisson denied', pexpect.EOF, pexpect.TIMEOUT])
                        if i == 0:
                                break
                        elif i == 1:
                                print "send password",
                                p.sendline(self.__password)
                        elif i == 2:
                                print 'Permisson denied'
                                p.close()
                                return None
                        else:
                                print "Failed to setup connection with %s" % receiver
				p.close()
				return None

		return p

	def readReport(self, receiver, taskId):
		q = self.__setupSshConn(receiver)
		if q is None:
			return "{}"
		jsonStr = json.dumps({'taskId':taskId})
		cmd = "python " + self.__codeDir + "/cmdReceiver.py -r \'" + jsonStr + "\'"
		q.sendline(cmd)
		i = q.expect(['readReport start', 'Usage error'])
		if i != 0:
			q.close()
			return "{}"
		q.expect(['{.*}'])
		result = q.after
		q.close()
		return result
		
	def createVolume(self, volCreator=None, receiver="", 
			 hostList=[], volName='testVol', brickPrefix='/exp', 
			 volType='distribute', count=1, transport='tcp'): 
		if hostList == []:
			print "hostList is empty"
                        return 1
		elif receiver == "":
			receiver = hostList[0]

		p = self.__setupSshConn(receiver)
		if p is None:
			return 1

		taskId = str(uuid.uuid1()) 
		kwparams = {'volCreator': volCreator, 
                            'hostList':hostList, 
                            'volName': volName, 
                            'volType': volType, 
                            'count': count, 
                            'brickPrefix':brickPrefix, 
                            'transport':transport,
			    'taskId': taskId}

		jsonStr = json.dumps(kwparams)
		cmd ="python " + self.__codeDir + "/cmdReceiver.py -C \'" + jsonStr + "\'"
		p.sendline(cmd)
		i = p.expect(['createVolume start', 'Usage error'])

	#	p.expect('end')
	#	print p.before
		if i != 0:
			print p.after
			p.close()
			return 1

		report = {}
		status = {}
		while True:
			report = self.readReport(receiver, taskId)
			print "The report of volumeCreate:"
			print report
			report = json.loads(report)
			if report != {}:
				if report['finished'] == True:
					self.__worker.update_progress(100, "volumeCreate is finished!")
					break	
				else:
					self.__worker.update_progress(int(Decimal(report['progress']) * 100), "volumeCreate is not finished yet!")
			time.sleep(1)
		p.close()

		if report['code'] == 0:
			mountJsonStr = json.dumps({'mount':report['outcome']})
			status = {'result': True, 
                                  'msg': '', 
                                  'data': mountJsonStr
			}
		elif report['code'] == -2:
			status = {'result': False,
                                  'msg': 'volumeCreate failed: no enough hosts to create a new volume',
                                  'data': ''
			}
		elif report['code'] == -3:
			status = {'result': False,
                                  'msg': 'volumeCreate failed: failed to create a new volume',
                                  'data': ''
			}
		elif report['code'] == -4:
			status = {'result': False,
                                  'msg': 'volumeCreate failed: failed to start the volume',
                                  'data': ''
			}
		else:
			status = {'result': False,
                                  'msg': 'volumeCreate failed: unkown type of failure',
                                  'data': ''
			}

		return json.dumps(status, sort_keys=True, indent=4)

	def replaceServer(self, receiver="", hostname=""):
		if hostname == "":
			print "hostname cannot be empty"
                        return 1
		if receiver=="":
			print "receiver cannont be empty"
			return 1

		p = self.__setupSshConn(receiver)
		if p is None:
			return 1

		taskId = str(uuid.uuid1()) 
		kwparams = {'hostname': hostname, 
			    'taskId': taskId}

		jsonStr = json.dumps(kwparams)
		cmd ="python "+self.__codeDir+"/cmdReceiver.py -R \'"+jsonStr+"\'"
		p.sendline(cmd)
		i = p.expect(['replaceServer start', 'Usage error'])
#		p.expect('end')
#		print p.before
		if i!=0:
			print p.after
			p.close()
			return 1

		report = {}
		status = {}
		while True:
			report = self.readReport(receiver, taskId)
			print "The report of replaceServer:"
			print report
			report = json.loads(report)
			if report != {}:
				if report['finished']==True:
					self.__worker.update_progress(100, "replaceServer is finished!")
					break	
				else:
					self.__worker.update_progress(int(Decimal(report['progress']) * 100), "replaceServer is not finished yet!")
			time.sleep(1)
		p.close()

		if report['code'] == 0:
                        status = {'result': True,
                                  'msg': '',
                                  'data': ''
                        }
                elif report['code'] == -1:
                        status = {'result': False,
                                  'msg': 'replaceServer failed: failed to get uuid',
                                  'data': ''
                        }
                elif report['code'] == -3:
                        status = {'result': False,
                                  'msg': 'replaceServer failed: failed to stop glusterd',
                                  'data': ''
                        }
                elif report['code'] == -4:
                        status = {'result': False,
                                  'msg': 'replaceServer failed: failed to write uuid to glusterd.info',
                                  'data': ''
                        }
		elif report['code'] == -5:
                        status = {'result': False,
                                  'msg': 'replaceServer failed: failed to first restart glusterd',
                                  'data': ''
                        }
		elif report['code'] == -6:
                        status = {'result': False,
                                  'msg': 'replaceServer failed: peer probe failed',
                                  'data': ''
                        }
		elif report['code'] == -7:
                        status = {'result': False,
                                  'msg': 'replaceServer failed: failed to second restart glusterd',
                                  'data': ''
                        }
                else:
                        status = {'result': False,
                                  'msg': 'replaceServer failed: unkown type of failure',
                                  'data': ''
                        }

                return json.dumps(status, sort_keys=True, indent=4)


	def triggerSelfHealing(self, receiver="", volName=""):
		if volName == "":
			print "volName cannot be empty"
                        return 1
		if receiver=="":
			print "receiver cannont be empty"
			return 1

		p = self.__setupSshConn(receiver)
		if p is None:
			return 1

		taskId = str(uuid.uuid1()) 
		kwparams = {'volName': volName, 
			    'taskId': taskId}

		jsonStr = json.dumps(kwparams)
		cmd ="python "+self.__codeDir+"/cmdReceiver.py -T \'"+jsonStr+"\'"
		p.sendline(cmd)
		i = p.expect(['triggerSelfHealing start', 'Usage error'])
#		p.expect('end')
#		print p.before
		if i!=0:
			print p.after
			p.close()
			return 1

		report = {}
		status = {}
		while True:
			report = self.readReport(receiver, taskId)
			print "The report of triggerSelfHealing:"
			print report
			report = json.loads(report)
			if report != {}:
				if report['finished'] == True:
					self.__worker.update_progress(100, "triggerSelfHealing is finished!")
					break	
				else:
					self.__worker.update_progress(int(Decimal(report['progress']) * 100), "triggerSelfHealing is not finished yet!")
			time.sleep(1)
		p.close()

		if report['code'] == 0:
                        status = {'result': True,
                                  'msg': '',
                                  'data': ''
                        }
                elif report['code'] == -1:
                        status = {'result': False,
                                  'msg': 'triggerSelfHealing failed: Execution of \'mkdir -p mountPoint\' failed',
                                  'data': ''
                        }
                elif report['code'] == -2:
                        status = {'result': False,
                                  'msg': 'triggerSelfHealing failed: failed to mount the volume',
                                  'data': ''
                        }
                elif report['code'] == -3:
                        status = {'result': False,
                                  'msg': 'triggerSelfHealing failed: Execution of self-healing failed',
                                  'data': ''
                        }
		elif report['code'] == -4:
                        status = {'result': False,
                                  'msg': 'triggerSelfHealing failed: failed to check the status of the volume',
                                  'data': ''
			}
                else:
                        status = {'result': False,
                                  'msg': 'triggerSelfHealing failed: unkown type of failure',
                                  'data': ''
                        }

                return json.dumps(status, sort_keys=True, indent=4)


if __name__ == '__main__':
	vc = volumeCreate(100)
	print "==========GUI volumeCreate==========\n"
        param1 = {'receiver': 'ntu01',
                  'hostList': ['ntu01', 'ntu02', 'ntu03'],
                  'volType': 'distribute',
                  'count': '10'
        }
        jsonStr1 = json.dumps(param1, sort_keys=True, indent=4)
        vc.run(jsonStr1)


        rs = replaceServer(200)
        print "==========GUI replaceServer==========\n"
	param2 = {'receiver': 'ntu01',
                  'hostname': 'ntu02'
	}
	jsonStr2 = json.dumps(param2, sort_keys=True, indent=4)
	rs.run(jsonStr2)


        sh = triggerSelfHealing(300)
        print "==========GUI triggerSelfHealing==========\n"
	param3 = {'receiver': 'ntu01',
                  'volName': 'testVol'
	}
	jsonStr3 = json.dumps(param3, sort_keys=True, indent=4)
	sh.run(jsonStr3)

