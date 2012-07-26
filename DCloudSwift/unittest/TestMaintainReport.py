import nose
import sys
import os
import json
import time

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from util.util import GlobalVar
from util import util
from util.database import MaintenanceBacklogDatabaseBroker
from util.MaintainReport import MaintainReport
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))


class TestMaintainReporter:
    def setup(self):
        self.DBFile = '/etc/test/maintain_report.db'
        os.system("rm -f %s" % self.DBFile)
        self.db = MaintenanceBacklogDatabaseBroker(self.DBFile)
        self.db.initialize()

    def testQueryAll(self):
        timestamp = int(time.mktime(time.localtime()))
        diskReserve = ['SN_reserve_001', 'SN_reserve_002', 'SN_reserve_003']
        diskReplace = ['SN_replace001', 'SN_replace002', 'SN_replace003']
        result = self.db.add_maintenance_task('node_missing', '192.168.2.30',
                        json.dumps(diskReserve), json.dumps(diskReplace))
        maintainReport = MaintainReport(self.DBFile)
        maintainReport.query()

    def testQueryTarget(self):
        timestamp = int(time.mktime(time.localtime()))
        diskReserve = ['SN_reserve_001', 'SN_reserve_002', 'SN_reserve_003']
        diskReplace = ['SN_replace001', 'SN_replace002', 'SN_replace003']
        result = self.db.add_maintenance_task('node_missing', '192.168.2.30',
                        json.dumps(diskReserve), json.dumps(diskReplace))
        maintainReport = MaintainReport(self.DBFile)
        maintainReport.query('node_missing')

    def teardown(self):
        os.system("rm -f %s" % self.DBFile)

