import nose
import sys
import os
import json
import string
import time
import subprocess

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from master.SwiftDeploy import SwiftDeploy
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))

class Test_isDeploymentOk:
	'''
	Test the function swiftDeploy.__isDeploymenOk in swiftDeploy.py
	'''
	def setup(self):
		self.SD = SwiftDeploy(WORKING_DIR+"/fake.ini")
		self.SD.updateNumOfReplica(3)
		self.numOfReplica = self.SD.getNumOfReplica()
		nose.tools.ok_(self.numOfReplica > 0, msg="number of Replica has to be greater than zero")

	def test_noEnoughZones(self):
		proxyList = []
		storageList = []
		blackList = []

		for i in range(10):
			ip = "192.168.12.%d"%i
			node = {"ip":ip}
			proxyList.append(node)

		for i in range(self.numOfReplica-1):
			ip = "192.168.12.%d"%i
			node = {"ip":ip, "zid":i}
			storageList.append(node)

		result = self.SD._SwiftDeploy__isDeploymentOk(proxyList, storageList, blackList)

		nose.tools.ok_(result.val==False)

	def test_noFailedNode(self):
		proxyList = []
		storageList = []
		blackList = []

		for i in range(10):
			ip = "192.168.12.%d"%i
			node = {"ip":ip}
			proxyList.append(node)

		for i in range(self.numOfReplica):
			ip = "192.168.12.%d"%i
			node = {"ip":ip, "zid":i}
			storageList.append(node)

		result = self.SD._SwiftDeploy__isDeploymentOk(proxyList, storageList, blackList)

		nose.tools.ok_(result.val==True)

	def test_tooManyFailedZones(self):
		proxyList = []
		storageList = []
		blackList = []

		for i in range(10):
			ip = "192.168.12.%d"%i
			node = {"ip":ip}
			proxyList.append(node)

		for i in range(10):
			ip = "192.168.12.%d"%i
			node = {"ip":ip, "zid":i}
			storageList.append(node)

		for i in range(self.numOfReplica):
			blackList.append("192.168.12.%d"%i)
			
		result = self.SD._SwiftDeploy__isDeploymentOk(proxyList, storageList, blackList)

		nose.tools.ok_(result.val==False)

	def test_tolerableNumberOfFailedZones(self):
		proxyList = []
		storageList = []
		blackList = []

		for i in range(10):
			ip = "192.168.12.%d"%i
			node = {"ip":ip}
			proxyList.append(node)

		for i in range(10):
			ip = "192.168.12.%d"%i
			node = {"ip":ip, "zid":i}
			storageList.append(node)

		for i in range(self.numOfReplica-1):
			blackList.append("192.168.12.%d"%i)
			
		result = self.SD._SwiftDeploy__isDeploymentOk(proxyList, storageList, blackList)
		nose.tools.ok_(result.val==True, result.msg)

	def test_allProxyFailed(self):
		proxyList = []
		storageList = []
		blackList = []

		for i in range(2):
			ip = "192.168.13.%d"%i
			node = {"ip":ip}
			proxyList.append(node)

		for i in range(10):
			ip = "192.168.12.%d"%i
			node = {"ip":ip, "zid":i}
			storageList.append(node)

		for i in range(2):
			blackList.append("192.168.13.%d"%i)
			
		result = self.SD._SwiftDeploy__isDeploymentOk(proxyList, storageList, blackList)
		nose.tools.ok_(result.val==False)

	def test_someProxyFailed(self):
		proxyList = []
		storageList = []
		blackList = []

		for i in range(3):
			ip = "192.168.13.%d"%i
			node = {"ip":ip}
			proxyList.append(node)

		for i in range(10):
			ip = "192.168.12.%d"%i
			node = {"ip":ip, "zid":i}
			storageList.append(node)

		for i in range(1):
			blackList.append("192.168.13.%d"%i)
			
		result = self.SD._SwiftDeploy__isDeploymentOk(proxyList, storageList, blackList)
		nose.tools.ok_(result.val==True, result.msg)

	def teardown(self):
		pass

if __name__ == "__main__":
	pass
