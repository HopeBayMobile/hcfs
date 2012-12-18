#!/usr/bin/env python
import nose
import sys
import os
import subprocess
import imp
import time
import ConfigParser

S3QL_AUTH_FILE = '/root/.s3ql/authinfo2'
S3QL_MNT_POINT = '/mnt/cloudgwfiles'
S3QL_CACHE_SIZE = 3072000
S3QL_ENTRY= 1500
S3QL_ARGS = '--allow-other --nfs --metadata-upload-interval 3600 ' \
            '--cachesize %d --max-cache-entries %d ' \
            '--compress lzma --authfile /root/.s3ql/authinfo2 --cachedir /root/.s3ql' % (S3QL_CACHE_SIZE, S3QL_ENTRY)

def _run_subprocess(cmd):
    """
    Utility function to run a command by subprocess.Popen

    @type cmd: string
    @param cmd: Command to run
    @rtype: tuple
    @return: Command return code and output string in a tuple
    """
    po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()

    return (po.returncode, output)
    
def _get_storage_url():
    """
    Get storage URL from /root/.s3ql/authinfo2.

    @rtype: string
    @return: Storage URL or None if failed.
    """
    storage_url = None

    try:
        config = ConfigParser.ConfigParser()
        with open(S3QL_AUTH_FILE) as op_fh:
            config.readfp(op_fh)

        section = "CloudStorageGateway"
        storage_url = config.get(section, 'storage-url')
    except Exception as e:
        print("Failed to getStorageUrl: %s" % str(e))
    
    return storage_url
    
def _get_storage_account():
    """
    Get storage account from auth file.
    @rtype: string
    @return: Storage account or "" if failed.
    """
    op_account = None
    
    try:
        op_config = ConfigParser.ConfigParser()
        with open(S3QL_AUTH_FILE) as op_fh:
            op_config.readfp(op_fh)
    
        section = "CloudStorageGateway"
        op_account = op_config.get(section, 'backend-login')
    except Exception as e:
        print("Failed to _get_storage_account: %s" % str(e))
    
    return op_account
       
class test_nas_like:
    """
    Test gateway NAS like function
    """
    def __init__(self):
        self.url = None
        self.port = None
        self.account = None
        self.usr_name = None
        self.swift_url = None
        
    def setup(self):
        print('Before running this test, please confirm the following changes:')
        print('    1. S3QL mount point will be umounted if it is already mounted')
        print('    2. All packets going out to Swift service port will be dropped')
        val = raw_input("Are you sure you want to proceed? (Y/n) ")
        assert val.lower() == 'y'

        # get storage url
        self.url = _get_storage_url()
        assert self.url != None
        
        # get service port of storage
        _, _, self.port = self.url.split(':')
        print('Swift service port: %s' % self.port)
        
        # get swift account
        self.account = _get_storage_account()
        assert self.account != None
        
        _, self.usr_name = self.account.split(':')
        assert self.usr_name != None
        
        self.swift_url = '%s/%s_private_container/delta' % (self.url, self.usr_name)

    def teardown(self):
        # unblock service port
        self.util_unblock_port()
    
    def util_mount_s3ql(self):
        # mount s3ql by our arguments
        print('Mounting S3QL with %dK cache size and %d entries...' % (S3QL_CACHE_SIZE, S3QL_ENTRY))
        _run_subprocess('sudo mount.s3ql %s %s %s' % (S3QL_ARGS, self.swift_url , S3QL_MNT_POINT))
        
    def util_umount_s3ql(self):
        print('Umounting S3QL...please wait')
        _run_subprocess('sudo umount.s3ql %s' % S3QL_MNT_POINT)
    
    def util_block_port(self, _port):
        # drop all packages going to port 443
        _run_subprocess('sudo iptables -A OUTPUT -p tcp -j DROP --dport %s -o eth1' % _port)
        
    def util_unblock_port(self):
        # delete assigned rules in iptables
        _run_subprocess('iptables -D OUTPUT 1')
        
    def util_gen_data(self, _size):
        print('Generating %dK test data...this takes a while' % _size)
        _run_subprocess('sudo dd if=/dev/urandom of=test.data bs=1024 count=%d' % _size)
    
    def test_01_write_full(self):
        print('Testing write data to make cache full...')
        # we need to change the cache size and entry number, 
        # thus if s3ql is mounted, we umount it and re-mount
        if os.path.ismount(S3QL_MNT_POINT):
            # umount s3ql
            self.util_umount_s3ql()
            
            # ensure s3ql is not mounted
            assert not os.path.ismount(S3QL_MNT_POINT)
            print('Umounted')
            
        # mount s3ql
        self.util_mount_s3ql()
        
        # ensure s3ql is mounted
        assert os.path.ismount(S3QL_MNT_POINT)
        print('Mounted')
        
        # block service port
        self.util_block_port(self.port)
        
        # generate test data
        data_gen = S3QL_CACHE_SIZE + 1024
        if os.path.exists('test.data'):
            statinfo = os.stat('test.data')
            if statinfo.st_size >= data_gen * 1024:
                print('Test data has been generated')
            else:
                self.util_gen_data(data_gen)
        else:
            self.util_gen_data(data_gen)
            
        # manually set upload bandwidth to < 1Gbps
        
        
        # copy test data
        print('Copying test data...')
        ret, _ = _run_subprocess('sudo cp test.data %s' % S3QL_MNT_POINT)
        print('Copied')
        
        # upload bandwidth should be full speed
        
        os.system('sudo rm -f %s/test.data' % S3QL_MNT_POINT)
        
        # unblock service port
        self.util_unblock_port()
        
        # result shouldn't be zero
        assert ret != 0
    
    def test_02_read_data_not_local(self):
        print('Testing read a data from remote storage...')
        assert os.path.ismount(S3QL_MNT_POINT)
        
        # generate test file
        os.system("sudo echo '111' > test.txt")
        
        # cp test file to s3ql mnt point
        os.system('sudo cp test.txt %s/' % S3QL_MNT_POINT)
        
        # flush s3ql cache
        print('Flushing S3QL cache...')
        os.system('sudo s3qlctrl flushcache %s' % S3QL_MNT_POINT)
        print('Flushed')
        
        # block service port
        self.util_block_port(self.port)
        
        # read file back, should fail
        ret, _ = _run_subprocess('sudo cp %s/test.txt .' % S3QL_MNT_POINT)
        
        # unblock service port
        self.util_unblock_port()
        
        # remove test file
        os.system("sudo rm -f %s/test.txt" % S3QL_MNT_POINT)
        
        assert ret != 0
        