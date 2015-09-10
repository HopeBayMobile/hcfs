#!/usr/bin/python
# -*- coding: utf-8 -*-
import os
import re
import time
import pdb
import subprocess
import logging
from subprocess import PIPE
from datetime import datetime

import swiftclient

logger = logging.getLogger('HCFS Tester')

class FSTester:
    """
    """
    def __init__(self, path_of_scripts):
        self.path_of_scripts = path_of_scripts
        self.harness = 'prove'
        
    def execute(self, test_type, script_filename=''):
        """Execute the fstest

        :param test_type: The type of test command ex. chmod
        :param script_filename: The test script file name.
        """
        target_script = os.path.join(os.path.join(self.path_of_scripts, test_type), script_filename)
        cmds = [self.harness, '-r', target_script]

        try:
            p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
            (output, error_msg) = p.communicate()
            print output
            result = True
        except OSError as e:
            result = False
            output = e
        except ValueError as e:
            result = False
            output = e

        return result, output

class FileGenerate:
    def __init__(self):
        self.block_num = 60

    def gen_block(self, content, size):
        """Generate the block with specific content. one block is 1024*1024

        :param content: data with string type, it only can use a singlge character.
        """
        block_data = ''
        if type(content) != type('1'):
            logger.error('Gen block failed!')
            return False
        else:
            for j in range(1, size+1, 1):
                block_data = block_data + content
            return block_data

    def gen_blocks(self, content, num, size):
        """Generate the blocks with specific content and number
        """
        blocks = ''
        for i in range(num):
            block = self.gen_block(content, size)
            if block:
                blocks = blocks + block
            else:
                logger.error('gen blocks failed')
                return False
        return blocks

    def gen_mix_blocks(self, content, num, size):
        """Generate the mixed blocks, it only contand '1' and '0' in the blocks.
        """
        blocks = ''
        for i in range(1, num+1):
            content = str(i % 2)
            block = self.gen_block(content, size)
            if block:
                blocks = blocks + block
            else:
                logger.error('gen blocks failed')
                return False
        return blocks

    def gen_file(self, filename, content, block_num, block_size, cwd):
        """Generate a file with the same block content.
        """
        if not filename or not block_num or not block_size:
            logger.error('Need input the parameter to API')
            return False

        file_path = os.path.join(cwd, filename)
        with open(file_path, 'wb') as f:
            blocks = self.gen_blocks(content, block_num, block_size)
            if blocks:
                f.write(blocks)
            else:
                logger.error('gen blocks failed')
                return False

    def gen_file_mix(self, filename, content, block_num, block_size, cwd):
        """Generate a file with mix block content
        """
        if not filename or not block_num or not block_size:
            logger.error('Need input the parameter to API')
            return False

        file_path = os.path.join(cwd, filename)
        with open(file_path, 'wb') as f:
            blocks = self.gen_mix_blocks(content, block_num, block_size)
            if blocks:
                f.write(blocks)
            else:
                logger.error('gen blocks failed')


class HCFSBin:
    """Wrap the HCFS executed file, that include hcfs and HCFSvol 
    """
    def __init__(self):
        self.current_dir = os.path.dirname(os.path.abspath(__file__))
        self.hcfs_bin_folder = os.path.join(self.current_dir, 'hcfs_bin')
        self.hcfs_bin = os.path.join(self.hcfs_bin_folder, 'hcfs')
        self.hcfsvol_bin = os.path.join(self.hcfs_bin_folder, 'HCFSvol')
        self.timeout = 30

    def start_hcfs(self):
        """Launch the hcfs processes
        """
        p = subprocess.Popen([self.hcfs_bin, '-d'])
        time.sleep(10)  # if use timeout for it?

    def verify_hcfs_processes(self):
        """Check the processes are exist, expect it has three.
        """
        count = 0
        p = subprocess.Popen(['ps', 'aux'], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()
        for line in  output.split('\n'):
            if re.search('hcfs', line):
                count = count + 1

        if count == 3:
            return True, ''
        else:
            return False, 'The process of hcfs number is wrong: {0}'.format(count)

    def check_if_hcfs_terminated(self):
        """Check the process is not exist
        """
        while (self.timeout > 0):
            process_exsit = 0    
            p = subprocess.Popen(['ps', 'aux'], stdout=PIPE, stderr=PIPE)
            (output, error_msg) = p.communicate()
            for line in  output.split('\n'):
                if re.search('hcfs', line):
                    process_exsit = 1

            time.sleep(1)
            if process_exsit == 1:
                self.timeout = self.timeout - 1
            else:
                break

        if process_exsit == 0:
            return True
        else:
            return False

    def terminate_hcfs(self):
        """Terminate the processes of hcfs
        """
        p = subprocess.Popen([self.hcfsvol_bin, 'terminate'], stdout=PIPE, stderr=PIPE)

    def create_filesystem(self, filesystem_name):
        """Create a filesystem of HCFS
        """
        p = subprocess.Popen([self.hcfsvol_bin, 'create', filesystem_name], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()
        if error_msg:
            return False, str(error_msg)
        else:
            return True, str(output)

    def mount(self, filesystem, mount_point):
        """Mount HCFS to a mount point
        """
        cmds = [self.hcfsvol_bin, 'mount', filesystem, mount_point]
        print cmds
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()
        if error_msg:
            return False, str(error_msg)
        else:
            return True, str(output)

    def restart_hcfs(self):
        """Restart the hcfs process
        """
        self.terminate_hcfs()
        self.start_hcfs()

class CommonSetup:
    def __init__(self):
        self.current_dir = os.path.dirname(os.path.abspath(__file__))
        self.fstest = FSTester('/mnt/hcfs/fstest')
        self.logger = logger
        # swift
        self.swift_url = 'https://10.10.99.118:8080/auth/v1.0'
        self.swift_key = '0qrrbOCHoNUM'
        self.swift_user = 'kentchen:kentchen'

    def exec_command_sync(self, commands, shell_flag):
        try:
            p = subprocess.Popen(commands, shell=shell_flag, stdout=PIPE, stderr=PIPE)
            (output, error_msg) = p.communicate()
        except OSError as e:
            output = e
        except ValueError as e:
            output = e
        print error_msg
        return output

    def replace_hcfs_config(self, source):
        p = subprocess.Popen(['cp', source, '/etc/hcfs.conf'], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if error_msg:
            self.logger(str(error_msg))
            return False, str(error_msg)
        else:
            return True, ''

    def get_container_size(self):
        cmds = ['swift', '--insecure', '-A', self.swift_url, '-U', self.swift_user, '-K', self.swift_key, 'stat']
        output = self.exec_command_sync(cmds, False)
        container_size = 0

        for i in output.split('\n'):
            if re.search(r'Bytes:', i):
                result = re.search('(\d+)', i)
                container_size = result.group(1)
        return container_size

    def copy_file_to_hcfs(self, testfilename_local, testfilename_hcfs):
        """Copy file to hcfs (upload)
        """
        p = subprocess.Popen(['cp', '-f', testfile_abs_local, testfile_abs_hcfs], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if error_msg:
            self.logger.error(str(error_msg))
            return False, error_msg
        else:
            return True, ''

    def copy_file_from_hcfs(self, testfilename_hcfs, testfilename_local):
        """Copy the file from hcfs (download)
        """
        p = subprocess.Popen(['cp', '-f', testfile_abs_hcfs, testfilename_local], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if error_msg:
            self.logger.error(str(error_msg))
            return False, error_msg
        else:
            return True, ''

    def diff_files(self, testfile_abs_local, testfile_abs_local_2):
        """Diff these two files
        """
        p = subprocess.Popen(['diff', '-q', testfile_abs_local, testfile_abs_local_2], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if output == '':
            return True, ''
        else:
            return False, output

    def check_hcfs_mount(self):
        p = subprocess.Popen(['mount'], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        for line in output.split('\n'):
            if re.search(r'hcfs', line):
                return True

        return False

    def check_hcfs_df(self):
        p = subprocess.Popen(['mount'], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        for line in output.split('\n'):
            if re.search(r'hcfs', line):
                return True

        return False


# ============================ Test Case Start ================================
class HCFS_0(CommonSetup):
    def __init__(self):
        CommonSetup.__init__(self)
        self.fake_filesystem = 'fakeHCFS'
        self.mount_point = '/mnt/hcfs2'
        self.HCFSBin = HCFSBin()

    def run(self):
        self.HCFSBin.start_hcfs()
        time.sleep(10)

        # create a filesystem
        (result, msg) = self.HCFSBin.create_filesystem(self.fake_filesystem)
        print result, msg

        # mount file system to a mount point
        (result, msg) = self.HCFSBin.mount(self.fake_filesystem, self.mount_point)
        print result, msg

        # check the df and mount
        if not self.check_hcfs_mount():
            return False, 'Mount failed'
        
        if not self.check_hcfs_df():
            return False, 'Missed df information'

        # copy fstest tool to HCFS

        return True, ''


class HCFS_3(CommonSetup):
    '''
    chflags
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'chflags'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg
    

class HCFS_4(CommonSetup):
    '''
    chmod
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'chmod'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg

class HCFS_5(CommonSetup):
    '''
    chmown
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'chown'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg

class HCFS_6(CommonSetup):
    '''
    link
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'link'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg

class HCFS_7(CommonSetup):
    '''
    mkdir
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'mkdir'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg

class HCFS_8(CommonSetup):
    '''
    chmod
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'mkfifo'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg 

class HCFS_9(CommonSetup):
    '''
    open
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'open'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg       

class HCFS_10(CommonSetup):
    '''
    rename
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'rename'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg


class HCFS_11(CommonSetup):
    '''
    rmdir
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'rmdir'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg


class HCFS_12(CommonSetup):
    '''
    symlink
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'symlink'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg


class HCFS_13(CommonSetup):
    '''
    truncate
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'truncate'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg

class HCFS_14(CommonSetup):
    '''
    unlink
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.type = 'unlink'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type)

        if 'Result: PASS' in output.split('\n'):
            result = True
            msg = ''
        else:
            result = False
            msg = output
        return result, msg


class HCFS_15(CommonSetup):
    '''
    Multiple mount
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''


class HCFS_16(CommonSetup):
    '''
    Backend Setting – ArkFlex U (Swift)
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.hcfs_bin = HCFSBin()
        self.config = 'hcfs_swift_easepro.conf'

    def check_if_hcfs_start_success(self):
        # Start the hcfs
        self.hcfs_bin.start_hcfs()

        # Check the process exist
        (result, msg) = self.hcfs_bin.verify_hcfs_processes()

        # close the hcfs
        self.hcfs_bin.terminate_hcfs()
        self.hcfs_bin.check_if_hcfs_terminated()

        return result, msg
        
    def run(self):
        self.replace_hcfs_config(self.config)
        (result, msg) = self.check_if_hcfs_start_success()

        return result, msg


class HCFS_17(CommonSetup):
    '''
    Backend Setting – ArkFlex U (S3)
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.hcfs_bin = HCFSBin()
        self.config = 'hcfs_s3_easepro.conf'
        
    def check_if_hcfs_start_success(self):
        # Start the hcfs
        self.hcfs_bin.start_hcfs()

        # Check the process exist
        (result, msg) = self.hcfs_bin.verify_hcfs_processes()

        # close the hcfs
        self.hcfs_bin.terminate_hcfs()
        self.hcfs_bin.check_if_hcfs_terminated()

        return result, msg

    def run(self):
        self.replace_hcfs_config(self.config)
        (result, msg) = self.check_if_hcfs_start_success()
        return result, msg


class HCFS_18(CommonSetup):
    '''
    Backend Setting – Swift (Pending)
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''


class HCFS_19(CommonSetup):
    '''
    Backend Setting – S3
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.hcfs_bin = HCFSBin()
        self.config = 'hcfs_s3.conf'
        self.hcfs_17 = HCFS_17()
        
    def run(self):
        self.replace_hcfs_config(self.config)
        (result, msg) = self.hcfs_17.check_if_hcfs_start_success()
        return result, msg
        return False, ''


class HCFS_20(CommonSetup):
    '''
    Upload – exceed soft limit
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate() 
        self.hcfsbin = HCFSBin()       
        self.mount_point = '/mnt/hcfs'
        self.config = 'hcfs_swift_easepro.conf'
        self.testfilename = 'testUpload_60M'
        self.first_time = ''
        self.new_time = ''
        self.log_timeformat = '%Y-%m-%d %H:%M:%s'

    def run(self):
        # Prepare the arkflex swift environment
        #self.replace_hcfs_config(self.config)
        #self.hcfsbin.restart_hcfs()

        # Get the size of the remote storage.
        origin_container_size = self.get_container_size()

        # Generate the 60M size file.
        self.fileGenerate.gen_file(self.testfilename, '1', 60, 1024*1024, self.current_dir)
        time.sleep(10)      # magic number again

        # Copy the file to hcfs (upload)
        testfile_abs_local = os.path.join(self.current_dir, self.testfilename)
        testfile_abs_hcfs = os.path.join('/mnt/hcfs', self.testfilename)
        self.first_time = datetime.now()
        p = subprocess.Popen(['cp', '-f', testfile_abs_local, testfile_abs_hcfs], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        # Diff these two files
        p = subprocess.Popen(['diff', '-q', testfile_abs_local, testfile_abs_hcfs], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        # Delete the generated file
        file_path = os.path.join(self.current_dir, testfile_abs_local)
        self.exec_command_sync(['rm', '-f', file_path], False)
        file_path = os.path.join(self.current_dir, testfile_abs_hcfs)
        self.exec_command_sync(['rm', '-f', file_path], False)

        if output == '':
            return True, ''
        else:
            return False, output

class HCFS_21(CommonSetup):
    '''
    Upload – exceed hard limit
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate() 
        self.hcfsbin = HCFSBin()       
        self.mount_point = '/mnt/hcfs'
        self.config = 'hcfs_swift_easepro.conf'
        self.testfilename = 'testUpload_160M'
        self.first_time = ''
        self.new_time = ''
        self.log_timeformat = '%Y-%m-%d %H:%M:%s'

    def run(self):
        # Prepare the arkflex swift environment
        #self.replace_hcfs_config(self.config)
        #self.hcfsbin.restart_hcfs()

        # Get the size of the remote storage.
        origin_container_size = self.get_container_size()

        # Generate the 60M size file.
        self.fileGenerate.gen_file(self.testfilename, '1', 160, 1024*1024, self.current_dir)
        time.sleep(10)      # magic number again

        # Copy the file to hcfs (upload)
        testfile_abs_local = os.path.join(self.current_dir, self.testfilename)
        testfile_abs_hcfs = os.path.join('/mnt/hcfs', self.testfilename)
        self.first_time = datetime.now()
        p = subprocess.Popen(['cp', '-f', testfile_abs_local, testfile_abs_hcfs], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        # Diff these two files
        p = subprocess.Popen(['diff', '-q', testfile_abs_local, testfile_abs_hcfs], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        # Delete the generated file
        file_path = os.path.join(self.current_dir, testfile_abs_local)
        self.exec_command_sync(['rm', '-f', file_path], False)
        file_path = os.path.join(self.current_dir, testfile_abs_hcfs)
        self.exec_command_sync(['rm', '-f', file_path], False)

        if output == '':
            return True, ''
        else:
            return False, output


class HCFS_22(CommonSetup):
    '''Upload/Download
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate() 
        self.hcfsbin = HCFSBin()       
        self.mount_point = '/mnt/hcfs'
        self.testfilename_local = 'testUpload_160M_local'
        self.testfilename_local2 = 'testUpload_160M_local2'
        self.testfilename_hcfs = 'testUpload_160M_hcfs'
        
    def run(self):
        # Get the size of the remote storage.
        origin_container_size = self.get_container_size()

        # Generate the 160M size file.
        self.fileGenerate.gen_file(self.testfilename_local, '1', 160, 1024*1024, self.current_dir)
        time.sleep(10)      # magic number again

        # Copy the file to hcfs (upload)
        testfile_abs_local = os.path.join(self.current_dir, self.testfilename_local)
        testfile_abs_hcfs = os.path.join(self.mount_point, self.testfilename_hcfs)
        p = subprocess.Popen(['cp', '-f', testfile_abs_local, testfile_abs_hcfs], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        # Copy the file from hcfs (download)
        testfile_abs_hcfs = os.path.join('/mnt/hcfs', self.testfilename_hcfs)
        testfile_abs_local_2 = os.path.join(self.current_dir, self.testfilename_local2)
        p = subprocess.Popen(['cp', '-f', testfile_abs_hcfs, testfile_abs_local_2], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        # Diff these two files
        p = subprocess.Popen(['diff', '-q', testfile_abs_local, testfile_abs_local_2], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        # Delete the generated file
        self.exec_command_sync(['rm', '-f', testfile_abs_local], False)
        self.exec_command_sync(['rm', '-f', testfile_abs_local_2], False)

        if output == '':
            return True, ''
        else:
            return False, output


class HCFS_39(CommonSetup):
    '''Upload big file
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate() 
        self.hcfsbin = HCFSBin()       
        self.mount_point = '/mnt/hcfs'
        self.samples_dir = '/home/kentchen/TestSamples'
        self.testfilename_local = 'matlabl2011-mac.iso'
        self.testfilename_local2 = 'matlabl2011-mac-2.iso'
        self.testfilename_hcfs = 'matlabl2011-mac.iso'

    def run(self):
        # Copy the file to hcfs (upload)
        testfile_abs_local = os.path.join(self.samples_dir, testfilename_local)
        testfile_abs_hcfs = os.path.join(mount_point, testfilename_hcfs)
        (result, error_msg) = copy_file_to_hcfs(self, testfile_abs_local, testfile_abs_hcfs)
        if not result:
            return False, error_msg

        # Copy the file from hcfs (download)
        testfile_abs_hcfs = os.path.join(mount_point, testfilename_hcfs)
        testfile_abs_local_2 = os.path.join(self.samples_dir, testfilename_local2)
        (result, error_msg) = copy_file_from_hcfs(self, testfilename_hcfs, testfilename_local2)
        if not result:
            return False, error_msg

        # Diff these two files
        (result, error_msg) = diff_files(self, testfile_abs_local, testfile_abs_local_2)
        if not result:
            return False, error_msg

        # Delete the generated file
        self.exec_command_sync(['rm', '-f', testfile_abs_local], False)
        self.exec_command_sync(['rm', '-f', testfile_abs_local_2], False)

class HCFS_99(CommonSetup):
    '''
    comment
    '''    
    def __init__(self):
        pass
        
    def run(self):

        return False, ''



if __name__ == '__main__':
    print os.path.dirname(os.path.abspath(__file__))
    hcfs_bin = HCFSBin()

        

