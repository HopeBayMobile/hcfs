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
from master.swiftEventMgr import SwiftEventMgr
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))


class Test_node_info:
    '''
    Test the function SwiftEventMgr.getExpectedDeviceCnt
    '''
    def setup(self):
        if os.path.exists("/etc/test/test.db"):
            os.system("rm /etc/test/test.db")
        self.db = NodeInfoDatabaseBroker("/etc/test/test.db")
        self.db.initialize()
        
        self.node = {"hostname": "nii3",
                     "status": "alive",
                     "timestamp": 123,
                     "disk": json.dumps({"missing":0}),
                     "mode": "service",
                     "switchpoint": 123,}

    def test_add_node_with_empty_disk(self):
        node = {"hostname": "nii",
                "status": "alive",
                "timestamp": 123,
                "disk": "",
                "mode": "service",
                "switchpoint": 123,}
        info = self.db.add_node(**node)
        nose.tools.ok_(info)

    @nose.tools.raises(Exception)
    def test_add_node_with_null_disk(self):
        node = {"hostname": "nii2",
                "status": "alive",
                "timestamp": 123,
                "disk": None,
                "mode": "service",
                "switchpoint": 123,}
        info = self.db.add_node(**node)

    def test_add_node(self):
        info = self.db.add_node(**(self.node))
        nose.tools.ok_(info)
        nose.tools.ok_(info["status"]== self.node["status"])
        nose.tools.ok_(info["hostname"]==self.node["hostname"])

    def test_update_node_status(self):
        info = self.db.add_node(**(self.node))
        self.db.add_node(**(self.node))
        info = self.db.update_node_status(hostname=self.node["hostname"], status="dead", timestamp=456)
        nose.tools.ok_(info)
        nose.tools.ok_(info["status"] == "dead")
        nose.tools.ok_(info["timestamp"] == 456)

    def test_get_info(self):
        info = self.db.add_node(**(self.node))
        self.db.add_node(**(self.node))
        info = self.db.get_info(self.node["hostname"])
        nose.tools.ok_(info["status"] == self.node["status"])

    def test_update_node_mode(self):
        info = self.db.add_node(**(self.node))
        info = self.db.update_node_mode(hostname=self.node["hostname"], mode="waiting", switchpoint=3889)
        nose.tools.ok_(info["mode"] == "waiting")
        nose.tools.ok_(info["switchpoint"] == 3889)

    def test_update_node_disk(self):
        info = self.db.add_node(**(self.node))
        info = self.db.update_node_disk(hostname=self.node["hostname"], disk="ooxx")
        nose.tools.ok_(info["disk"] == "ooxx")

    def test_delete_node(self):
        info = self.db.add_node(**(self.node))
        info = self.db.delete_node(hostname=self.node["hostname"])
        nose.tools.ok_(info["hostname"] == self.node["hostname"])
        info = self.db.get_info(hostname=self.node["hostname"])
        nose.tools.ok_(not info)

    def teardown(self):
        os.system("rm /etc/test/test.db")

