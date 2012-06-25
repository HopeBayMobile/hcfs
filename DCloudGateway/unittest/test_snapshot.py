import nose
import sys
import os.path
import json
import random
import time
import threading
import ctypes
from ConfigParser import ConfigParser

# Add gateway sources
DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
BASEDIR = os.path.dirname(DIR)
sys.path.insert(0, os.path.join(BASEDIR, 'src'))

# Import packages to be tested
from gateway import snapshot
from gateway.common import TimeoutError
from gateway.common import timeout

test_snapshot_path = '/mnt/cloudgwfiles/sambashare/testing_snapshot'
snapshot_tag = "/root/.s3ql/.snapshotting"
snapshot_dir = "/mnt/cloudgwfiles/snapshots"
snapshot_db = "/root/.s3ql/snapshot_db.txt"
temp_folder = "/mnt/cloudgwfiles/tempsnapshot"


class SnapshotError(Exception):
    pass


class Test_takesnapshot:
    '''
    Test a quick snapshot.
    '''
    def setup(self):
        '''
        Make a new dir in sambashare and touch a new file.
        '''
        if os.path.exists(test_snapshot_path):  # Not a clean setup
            os.system('sudo rm -rf %s' % test_snapshot_path)

        if os.path.exists(snapshot_tag):
            os.system('sudo rm -rf %s' % snapshot_tag)

        os.system('sudo mkdir %s' % test_snapshot_path)
        self.newfile = os.path.join(test_snapshot_path, 'testfile')

        os.system('sudo touch %s' % self.newfile)


    def teardown(self):
        '''
        Delete the testing directory
        '''
        if not os.path.exists(self.newfile):  # Test file already deleted?
            raise SnapshotError
        os.system('sudo rm -rf %s' % test_snapshot_path)
        # Delete the snapshot database
        if os.path.exists(snapshot_db):
            os.system('sudo rm -rf %s' % snapshot_db)
        if os.path.exists(temp_folder):
            os.system('sudo rm -rf %s' % temp_folder)

    def test_inprogress(self):
        '''
        Test if can detect snapshot-in-progress
        '''
        if os.path.exists(snapshot_tag):  # Someone else is doing a snapshot?
            raise SnapshotError

        os.system('sudo touch %s' % snapshot_tag)

        result = snapshot.get_snapshot_in_progress()
        result_val = json.loads(result)
        nose.tools.eq_(result_val['data']['in_progress'], 'new_snapshot')

        os.system('sudo rm -rf %s' % snapshot_tag)

    def test_takesnapshot(self):
        '''
        Test taking snapshots
        '''
        if os.path.exists(snapshot_tag):  # Someone else is doing a snapshot?
            raise SnapshotError

        result = snapshot.get_snapshot_list()
        result_val = json.loads(result)
        old_snapshot_list = result_val['data']['snapshots']

        result = snapshot.take_snapshot()
        result_val = json.loads(result)
        nose.tools.eq_(result_val['result'], True)

        finished = False

        retries = 10

        # Wait for the snapshotting process to finish
        while not finished:
            retries = retries - 1
            result = snapshot.get_snapshot_list()
            result_val = json.loads(result)
            new_snapshot_list = result_val['data']['snapshots']

            if len(new_snapshot_list) > 0:
                newest_snapshot = new_snapshot_list[0]['name']

                if newest_snapshot != 'new_snapshot':
                    finished = True

            if retries < 0:
                raise SnapshotError('Waited too long for snapshots')
            time.sleep(5)

        # Test if the snapshot is taken correctly
        nose.tools.ok_(new_snapshot_list[0]['finish_time'] > 0)
        nose.tools.eq_(new_snapshot_list[0]['exposed'], False)
        test_file_path = os.path.join(snapshot_dir, newest_snapshot)
        test_file_name = os.path.join(test_file_path,\
                           'sambashare/testing_snapshot/testfile')
        nose.tools.ok_(os.path.exists(test_file_name))

        # Let's now delete the snapshot

        result = snapshot.delete_snapshot(newest_snapshot)
        result_val = json.loads(result)

        nose.tools.eq_(result_val['result'], True)

        nose.tools.ok_(not os.path.exists(test_file_name))

        result = snapshot.get_snapshot_list()
        result_val = json.loads(result)
        new_snapshot_list = result_val['data']['snapshots']

        if len(new_snapshot_list) > 0:
            nose.tools.ok_(new_snapshot_list[0]['name'] != newest_snapshot)

        nose.tools.ok_(os.path.exists(snapshot_dir))
