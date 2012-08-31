import nose
import sys
import os.path
import json
import random
import time
import threading
import ctypes
import subprocess
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
snapshot_schedule = "/etc/delta/snapshot_schedule"
lifespan_conf = "/etc/delta/snapshot_lifespan"
DB_FILE_BAK = "/root/.s3ql/snapshot_db.txt.tmp"


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

        if os.path.exists(snapshot_db):
            os.system('sudo cp %s /root/.s3ql/.backup_snapshot_database' % snapshot_db)
        if os.path.exists(snapshot_schedule):
            os.system('sudo cp %s /etc/delta/.backup_snapshot_schedule' % snapshot_schedule)
        if os.path.exists(lifespan_conf):
            os.system('sudo cp %s /etc/delta/.backup_snapshot_lifespan' % lifespan_conf)

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

        # Restore the snapshot database
        if os.path.exists('/root/.s3ql/.backup_snapshot_database'):
            os.system('sudo cp /root/.s3ql/.backup_snapshot_database %s' % snapshot_db)
            os.system('sudo rm -rf /root/.s3ql/.backup_snapshot_database')

        if os.path.exists('/etc/delta/.backup_snapshot_schedule'):
            os.system('sudo cp /etc/delta/.backup_snapshot_schedule %s' % snapshot_schedule)
            os.system('sudo rm -rf /etc/delta/.backup_snapshot_schedule')

        if os.path.exists('/etc/delta/.backup_snapshot_lifespan'):
            os.system('sudo cp /etc/delta/.backup_snapshot_lifespan %s' % lifespan_conf)
            os.system('sudo rm -rf /etc/delta/.backup_snapshot_lifespan')

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
        old_len = len(old_snapshot_list)

        result = snapshot.take_snapshot()
        result_val = json.loads(result)
        nose.tools.eq_(result_val['result'], True)
        result = snapshot.get_snapshot_list()
        result_val = json.loads(result)
        new_snapshot_list = result_val['data']['snapshots']
        new_len = len(new_snapshot_list)
        nose.tools.ok_(new_len > old_len)

        finished = False

        retries = 10

        # Wait for the snapshotting process to finish
        while not finished:
            retries = retries - 1
            result = snapshot.get_snapshot_list()
            result_val = json.loads(result)
            new_snapshot_list = result_val['data']['snapshots']
            new_len = len(new_snapshot_list)

            if new_len > 0 and new_len > old_len:
                newest_snapshot = new_snapshot_list[0]['name']

                if newest_snapshot != 'new_snapshot':
                    finished = True

            if retries < 0:
                raise SnapshotError('Waited too long for snapshots')
            time.sleep(5)

        # Test if the snapshot is taken correctly
        nose.tools.ok_(new_snapshot_list[0]['finish_time'] > 0)
        # wthung, 2012/7/18
        # by auto-exposed feature, "exposed" is False and "auto_exposed" is True by default
        nose.tools.eq_(new_snapshot_list[0]['exposed'], False)
        nose.tools.eq_(new_snapshot_list[0]['auto_exposed'], True)
        test_file_path = os.path.join(snapshot_dir, newest_snapshot)
        test_file_name = os.path.join(test_file_path,\
                           'sambashare/testing_snapshot/testfile')
        nose.tools.ok_(os.path.exists(test_file_name))

        # wthung, 2012/7/18
        # since snapshot is auto-exposed, we will try to delete directly
        
        # Let's now share this snapshot
        #result = snapshot.expose_snapshot([newest_snapshot])
        # Attempt to delete this snapshot will fail
        result = snapshot.delete_snapshot(newest_snapshot)
        result_val = json.loads(result)
        nose.tools.eq_(result_val['result'], False)
        # Let's now not expose the snapshot
        result = snapshot.expose_snapshot([])

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


    def test_snapshot_schedule_config(self):
        '''
        Test getting and setting snapshot schedule config file.
        '''

        os.system('sudo rm -rf %s' % snapshot_schedule)
        result_tmp = snapshot.get_snapshot_schedule()
        result = json.loads(result_tmp)
        nose.tools.ok_(os.path.exists(snapshot_schedule))
        nose.tools.eq_(result['data']['snapshot_time'], 1)

        result = snapshot.set_snapshot_schedule(22)
        result_tmp = snapshot.get_snapshot_schedule()
        result = json.loads(result_tmp)
        nose.tools.eq_(result['data']['snapshot_time'], 22)


    def test_snapshot_lifespan_config(self):
        '''
        Test getting and setting snapshot lifespan config file.
        '''

        os.system('sudo rm -rf %s' % lifespan_conf)
        result_tmp = snapshot.get_snapshot_lifespan()
        result = json.loads(result_tmp)
        nose.tools.ok_(os.path.exists(lifespan_conf))
        nose.tools.eq_(result['data']['days_to_live'], 365)

        result = snapshot.set_snapshot_lifespan(100)
        result_tmp = snapshot.get_snapshot_lifespan()
        result = json.loads(result_tmp)
        nose.tools.eq_(result['data']['days_to_live'], 100)
    
    def _backup_snapshot_DB(self):
        # backup existing db file if any
        if os.path.exists(snapshot_db):
            os.system('sudo mv %s %s' % (snapshot_db, DB_FILE_BAK))
    
    def _restore_snapshot_DB(self):
        # restore backup db file if any
        if os.path.exists(DB_FILE_BAK):
            os.system('sudo mv %s %s' % (DB_FILE_BAK, snapshot_db))
    
    def test_get_snapshot_last_status(self):
        """
        Test get status of last snapshot.
        """

        self._backup_snapshot_DB()
        
        #---------------------------------------------------------------------
        # create temp db file
        os.system('sudo touch %s' % snapshot_db)
        with open(snapshot_db, 'w') as fh:
            fh.write('snapshot_2012_7_7_7_7_7,100,-1,10,10,true,true\n')
        
        # expect false result
        result_val = json.loads(snapshot.get_snapshot_last_status())
        nose.tools.eq_(result_val['result'], False)
        nose.tools.eq_(result_val['latest_snapshot_time'], -1)
        
        # delete temp db file
        os.system('sudo rm -rf %s' % snapshot_db)
        
        #---------------------------------------------------------------------
        # create temp db file
        os.system('sudo touch %s' % snapshot_db)
        with open(snapshot_db, 'w') as fh:
            fh.write('snapshot_2012_7_7_7_7_7,100,120,10,10,true,true\n')
            
        # expect true result
        result_val = json.loads(snapshot.get_snapshot_last_status())
        nose.tools.eq_(result_val['result'], True)
        nose.tools.eq_(result_val['latest_snapshot_time'], 120)
        
        # delete temp db file
        os.system('sudo rm -rf %s' % snapshot_db)
        
        #---------------------------------------------------------------------
        
        # create temp db file, but no content (emulate no snapshot)
        os.system('sudo touch %s' % snapshot_db)
            
        # expect true result
        result_val = json.loads(snapshot.get_snapshot_last_status())
        nose.tools.eq_(result_val['result'], True)
        nose.tools.eq_(result_val['latest_snapshot_time'], -1)
        
        # delete temp db file
        os.system('sudo rm -rf %s' % snapshot_db)
        
        #---------------------------------------------------------------------
        self._restore_snapshot_DB()
    
    def test_rebuild_snapshot_database(self):
        """
        Test rebuild snapshot database
        """
        
        # prepare snapshot dirs
        ss_tmp_dir = '/tmp/snapshots'
        if not os.path.exists(ss_tmp_dir):
            os.system('sudo mkdir %s' % ss_tmp_dir)
            
        tmp_dir1 = 'snapshot_2012_9_9_9_9_9'
        tmp_dir2 = 'snapshot_2012_9_9_9_9_10'
        
        os.system('sudo mkdir %s/%s' % (ss_tmp_dir, tmp_dir1))
        os.system('sudo mkdir %s/%s/sambashare' % (ss_tmp_dir, tmp_dir1))
        os.system('sudo mkdir %s/%s/nfsshare' % (ss_tmp_dir, tmp_dir1))
        os.system('sudo mkdir %s/%s' % (ss_tmp_dir, tmp_dir2))
        os.system('sudo mkdir %s/%s/sambashare' % (ss_tmp_dir, tmp_dir2))
        os.system('sudo mkdir %s/%s/nfsshare' % (ss_tmp_dir, tmp_dir2))
        
        # create some file into tmp dir
        os.system("sudo dd if=/dev/zero of=%s/%s/sambashare/test.file bs=1024 count=1" % (ss_tmp_dir, tmp_dir1))
        os.system("sudo touch %s/%s/dummy.file1" % (ss_tmp_dir, tmp_dir1))
        os.system("sudo dd if=/dev/zero of=%s/%s/sambashare/test.file bs=1024 count=2" % (ss_tmp_dir, tmp_dir2))
        os.system("sudo touch %s/%s/dummy.file1" % (ss_tmp_dir, tmp_dir2))
        
        self._backup_snapshot_DB()
        snapshot.rebuild_snapshot_database(ss_tmp_dir)
        
        # get file lines of snapshot db
        cmd = "sudo wc -l %s" % snapshot_db
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        
        line_num = output.split(' ')
        nose.tools.eq_(line_num[0], "2")
        
        # check file number
        with open(snapshot_db, 'r') as fh:
            for line in fh:
                name, start_time, end_time, file_num, total_size, exposed, auto_exposed = line.split(',')
                nose.tools.eq_(file_num, "2")
    
        self._restore_snapshot_DB()
        os.system('sudo rm -rf %s' % ss_tmp_dir)
        
