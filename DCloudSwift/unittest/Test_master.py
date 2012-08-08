import nose
import sys
import os
import json
import time

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from master.SwiftDeploy import SwiftDeploy
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))


class Test_isDeploymentOk:
    '''
    Test the function swiftDeploy.__isDeploymenOk in swiftDeploy.py
    '''
    def setup(self):
        self.SD = SwiftDeploy(WORKING_DIR + "/fake.ini")
        self.numOfReplica = 3

    def test_noEnoughZones(self):
        proxyList = []
        storageList = []
        blackList = []

        for i in range(10):
            ip = "192.168.12.%d" % i
            node = {"ip": ip}
            proxyList.append(node)

        for i in range(self.numOfReplica - 1):
            ip = "192.168.12.%d" % i
            node = {"ip": ip, "zid": i}
            storageList.append(node)

        result = self.SD.isDeploymentOk(proxyList, storageList, blackList, self.numOfReplica)

        nose.tools.ok_(result.val == False)

    def test_noFailedNode(self):
        proxyList = []
        storageList = []
        blackList = []

        for i in range(10):
            ip = "192.168.12.%d" % i
            node = {"ip": ip}
            proxyList.append(node)

        for i in range(self.numOfReplica):
            ip = "192.168.12.%d" % i
            node = {"ip": ip, "zid": i}
            storageList.append(node)

        result = self.SD.isDeploymentOk(proxyList, storageList, blackList, self.numOfReplica)

        nose.tools.ok_(result.val == True)

    def test_tooManyFailedZones(self):
        proxyList = []
        storageList = []
        blackList = []

        for i in range(10):
            ip = "192.168.12.%d" % i
            node = {"ip": ip}
            proxyList.append(node)

        for i in range(10):
            ip = "192.168.12.%d" % i
            node = {"ip": ip, "zid": i}
            storageList.append(node)

        for i in range(self.numOfReplica):
            blackList.append("192.168.12.%d" % i)

        result = self.SD.isDeploymentOk(proxyList, storageList, blackList, self.numOfReplica)

        nose.tools.ok_(result.val == False)

    def test_tolerableNumberOfFailedZones(self):
        proxyList = []
        storageList = []
        blackList = []

        for i in range(10):
            ip = "192.168.12.%d" % i
            node = {"ip": ip}
            proxyList.append(node)

        for i in range(10):
            ip = "192.168.12.%d" % i
            node = {"ip": ip, "zid": i}
            storageList.append(node)

        for i in range(self.numOfReplica - 1):
            blackList.append("192.168.12.%d" % i)

        result = self.SD.isDeploymentOk(proxyList, storageList, blackList, self.numOfReplica)
        nose.tools.ok_(result.val == True, result.msg)

    def test_allProxyFailed(self):
        proxyList = []
        storageList = []
        blackList = []

        for i in range(2):
            ip = "192.168.13.%d" % i
            node = {"ip": ip}
            proxyList.append(node)

        for i in range(10):
            ip = "192.168.12.%d" % i
            node = {"ip": ip, "zid": i}
            storageList.append(node)

        for i in range(2):
            blackList.append("192.168.13.%d" % i)

        result = self.SD.isDeploymentOk(proxyList, storageList, blackList, self.numOfReplica)
        nose.tools.ok_(result.val == False)

    def test_someProxyFailed(self):
        proxyList = []
        storageList = []
        blackList = []

        for i in range(3):
            ip = "192.168.13.%d" % i
            node = {"ip": ip}
            proxyList.append(node)

        for i in range(10):
            ip = "192.168.12.%d" % i
            node = {"ip": ip, "zid": i}
            storageList.append(node)

        for i in range(1):
            blackList.append("192.168.13.%d" % i)

        result = self.SD.isDeploymentOk(proxyList, storageList, blackList, self.numOfReplica)
        nose.tools.ok_(result.val == True, result.msg)

    def teardown(self):
        pass


class Test_isDeletionOfNodesSafe:
    '''
    Test the function swiftDeploy.isDeletionOfNodesSafe in swiftDeploy.py
    '''
    def setup(self):
        self.SD = SwiftDeploy(WORKING_DIR + "/fake.ini")
        self.swiftDir = WORKING_DIR + '/test_config/swift'

    def test_unSafeDeletion(self):
        ipList = ["192.168.11.6", "192.168.11.7", "192.168.11.10"]

        result = self.SD.isDeletionOfNodesSafe(ipList, self.swiftDir)
        nose.tools.ok_(result.val == False)

    def test_safeDeletion(self):
        ipList = ["192.168.11.6", "192.168.11.10"]

        result = self.SD.isDeletionOfNodesSafe(ipList, self.swiftDir)
        nose.tools.ok_(result.val == True)

    def test_emptyIpList(self):
        ipList = []

        result = self.SD.isDeletionOfNodesSafe(ipList, self.swiftDir)
        nose.tools.ok_(result.val == True)

    def test_nonExistentIp(self):
        ipList = ["192.16.11.19", "192.168.11.6"]

        result = self.SD.isDeletionOfNodesSafe(ipList, self.swiftDir)
        nose.tools.ok_(result.val == True)

    def test_duplicateIp(self):
        ipList = ["192.168.11.6", "192.168.11.6", "192.168.11.7"]

        result = self.SD.isDeletionOfNodesSafe(ipList, self.swiftDir)
        nose.tools.ok_(result.val == True)

    def test_nonExistentSwiftDir(self):
        ipList = ["192.168.11.6", "192.168.11.6", "192.168.11.7"]

        result = self.SD.isDeletionOfNodesSafe(ipList, "/nonExistentSwiftDir")
        nose.tools.ok_(result.val == False)

    def teardown(self):
        pass


if __name__ == "__main__":
    pass
