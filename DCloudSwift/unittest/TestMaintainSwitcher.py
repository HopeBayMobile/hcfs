import nose
import sys
import os
import json
import time

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from util.util import GlobalVar
from util import util
from util.database import NodeInfoDatabaseBroker
from master.SwiftMaintainSwitcher import SwiftMaintainSwitcher
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))


class TestMaintainSwitcher:
    def setup(self):
        self.DBFile = '/etc/test/maintain_switcher.db'
        os.system("rm -f %s" % self.DBFile)
        self.db = NodeInfoDatabaseBroker(self.DBFile)
        self.db.initialize()

    def testCheckServiceStatusDead(self):
        timestamp = int(time.mktime(time.localtime()))
        diskInfo = {
                    'timestamp': timestamp,
                    'missing': {
                                    'count': 6,
                                    'timestamp': timestamp,
                                },
                    'broken': [
                               {
                                    'SN': 'aaaaa',
                                    'timestamp': timestamp,
                               },
                              ],
                    'healthy': [
                                {
                                    'SN': 'bbbbb',
                                    'timestamp': timestamp,
                                }
                               ],
                }
        result = self.db.add_node('192.168.1.20', 'dead', timestamp-100,
                                  json.dumps(diskInfo), 'service', timestamp)
        daemon = SwiftMaintainSwitcher('/var/run/SwiftMaintainSwitcher.pid',
                                       self.DBFile, 1, 1, 1)
        daemon.checkService()
        serviceNode = self.db.show_node_info_table()
        for row in serviceNode:
            nose.tools.eq_(row['hostname'], result['hostname'])
            nose.tools.eq_(row['status'], result['status'])
            nose.tools.eq_(row['timestamp'], result['timestamp'])
            nose.tools.eq_(row['disk'], result['disk'])
            nose.tools.eq_(row['mode'], 'waiting')
            nose.tools.ok_((row['switchpoint'] >= result[5]))

    def testCheckServiceStatusAliveLargeThanRefreshTime(self):
        timestamp = int(time.mktime(time.localtime()))
        diskInfo = {
                    'timestamp': timestamp,
                    'missing': {
                                    'count': 6,
                                    'timestamp': timestamp,
                                },
                    'broken': [
                               {
                                    'SN': 'aaaaa',
                                    'timestamp': timestamp,
                               },
                              ],
                    'healthy': [
                                {
                                    'SN': 'aaaaa',
                                    'timestamp': timestamp,
                                }
                               ],
                }
        result = self.db.add_node('192.168.1.20', 'alive', timestamp-100,
                                  json.dumps(diskInfo), 'service', timestamp)
        daemon = SwiftMaintainSwitcher('/var/run/SwiftMaintainSwitcher.pid',
                                       self.DBFile, 1, 1, 1)
        daemon.checkService()
        serviceNode = self.db.show_node_info_table()
        for row in serviceNode:
            nose.tools.eq_(row['hostname'], result['hostname'])
            nose.tools.eq_(row['status'], 'dead')
            nose.tools.ok_(row['timestamp'] > result['timestamp'])
            nose.tools.eq_(row['disk'], result['disk'])
            nose.tools.eq_(row['mode'], 'waiting')
            nose.tools.ok_((row['switchpoint'] >= result['switchpoint']))

    def testCheckServiceStatusAliveDiskError(self):
        timestamp = int(time.mktime(time.localtime()))
        diskInfo = {
                    'timestamp': timestamp,
                    'missing': {
                                    'count': 6,
                                    'timestamp': timestamp-1,
                                },
                    'broken': [
                               {
                                    'SN': 'aaaaa',
                                    'timestamp': timestamp-1,
                               },
                              ],
                    'healthy': [
                                {
                                    'SN': 'aaaaa',
                                    'timestamp': timestamp,
                                }
                               ],
                }
        result = self.db.add_node('192.168.1.20', 'alive', timestamp,
                                  json.dumps(diskInfo), 'service', timestamp)
        daemon = SwiftMaintainSwitcher('/var/run/SwiftMaintainSwitcher.pid',
                                       self.DBFile, 1, 100, 1)
        daemon.checkService()
        serviceNode = self.db.show_node_info_table()
        for row in serviceNode:
            nose.tools.eq_(row['hostname'], result['hostname'])
            nose.tools.eq_(row['status'], result['status'])
            nose.tools.eq_(row['timestamp'], result['timestamp'])
            nose.tools.eq_(row['disk'], result['disk'])
            nose.tools.eq_(row['mode'], 'waiting')
            nose.tools.ok_((row['switchpoint'] >= result['switchpoint']))

    def testCheckWaitingStatusAliveNoDiskError(self):
        timestamp = int(time.mktime(time.localtime()))
        diskInfo = {
                    'timestamp': timestamp,
                    'missing': {
                                    'count': 6,
                                    'timestamp': timestamp,
                                },
                    'broken': [
                               {
                                    'SN': 'aaaaa',
                                    'timestamp': timestamp,
                               },
                              ],
                    'healthy': [
                                {
                                    'SN': 'aaaaa',
                                    'timestamp': timestamp,
                                }
                               ],
                }
        result = self.db.add_node('192.168.1.20', 'alive', timestamp-10,
                                  json.dumps(diskInfo), 'waiting', timestamp)
        daemon = SwiftMaintainSwitcher('/var/run/SwiftMaintainSwitcher.pid',
                                       self.DBFile, 1, 100, 1)
        daemon.checkWaiting()
        serviceNode = self.db.show_node_info_table()
        print serviceNode
        print result
        for row in serviceNode:
            nose.tools.eq_(row['hostname'], result['hostname'])
            nose.tools.eq_(row['status'], result['status'])
            nose.tools.eq_(row['timestamp'], result['timestamp'])
            nose.tools.eq_(row['disk'], result['disk'])
            nose.tools.eq_(row['mode'], 'service')
            nose.tools.ok_((row['switchpoint'] >= result['switchpoint']))

    def teardown(self):
        os.system("rm -f %s" % self.DBFile)

