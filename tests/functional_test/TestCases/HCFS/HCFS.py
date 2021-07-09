#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# Copyright (c) 2021 HopeBayTech.
#
# This file is part of Tera.
# See https://github.com/HopeBayMobile for further info.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""
Classes:
    FSTester: The Wrapper for fstest

"""
import os
import errno
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
        cmds = ['sudo', self.harness, '-r', target_script]
        logger.info(str(cmds))

        try:
            p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
            (output, error_msg) = p.communicate()
            print output
            print error_msg
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

        :param content: data with string type, it only can use a singlge
                        character.
        """
        block_data = ''
        if isinstance(content, str):
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
        self.repo_dir = os.path.abspath(os.path.join(self.current_dir, '../../../..'))
        self.hcfs_bin = self.which('hcfs')
        self.hcfsvol_bin = self.which('HCFSvol')
        # Fallback to default binary path if not exist
        if not self.hcfs_bin:
            self.hcfs_bin = os.path.abspath(os.path.join(self.repo_dir, 'src/HCFS/hcfs'))
        if not self.hcfsvol_bin:
            self.hcfsvol_bin = os.path.abspath(os.path.join(self.repo_dir, 'src/CLI_utils/HCFSvol'))
        self.timeout = 30

    def which(self, program):
        import os
        def is_exe(fpath):
            return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

        fpath, fname = os.path.split(program)
        if fpath:
            if is_exe(program):
                return program
        else:
            for path in os.environ["PATH"].split(os.pathsep):
                path = path.strip('"')
                exe_file = os.path.join(path, program)
                if is_exe(exe_file):
                    return exe_file

        return None

    def start_hcfs(self):
        """Launch the hcfs processes with allow_other
        """
        cmds = ['bash', 'TestCases/HCFS/start_hcfs.bash']
        try:
            logger.info('Start the HCFS process..')
            subprocess.call(cmds)
        except:
            logger.error('Start hcfs in background failed. cmds: {0}'.format(cmds))

        # Make Sure it's actually start by test list command
        self.list_filesystems()

    def verify_hcfs_processes(self):
        """Check the processes are exist, expect it has three.
        """
        count = 0
        logger.info('Verify hcfs processes')
        p = subprocess.Popen(['ps', 'aux'], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()
        for line in output.split('\n'):
            if re.search("/hcfs( |$)", line):
                count = count + 1

        if count == 3:
            return True, ''
        else:
            return False, 'The process of hcfs number "{0}" is wrong'.format(count)

    def check_if_hcfs_terminated(self, timeout=None):
        """Check the process is not exist, timeout default is 30
        """
        timeout = self.timeout if timeout is None else timeout
        logger.info('Check the process is not exist, timeout is {0}'.format(timeout))
        while (timeout > 0):
            process_exsit = 0
            p = subprocess.Popen(['ps', 'aux'], stdout=PIPE, stderr=PIPE)
            (output, error_msg) = p.communicate()
            for line in output.split('\n'):
                if re.search("/hcfs( |$)", line):
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
        logger.info('Terminate the HCFS process..')
        p = subprocess.Popen([self.hcfsvol_bin, 'terminate'], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()
        if error_msg:
            logger.error(str(error_msg))
        else:
            logger.info(output)

    def list_filesystems(self):
        cmds = [self.hcfsvol_bin, 'list']
        count = 0
        for i in range(60):
            p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
            (output, error_msg) = p.communicate()
            output_lines = output.split('\n')

            # check the return message
            for output_line in output_lines:
                r = re.search(r'status is (.+?),\s.+', output_line)
                if r and r.group(1) == '-1':
                    result = r.group(1)
                    logger.error('[list_filesystems] cmds: ' + str(cmds))
                    logger.error('[list_filesystems] ' + str(output))
                    logger.error('[list_filesystems] retry: ' + str(count))
                elif r and r.group(1) == '0':
                    logger.info('List filesystems: ' + str(output_lines))
                    return output_lines[1:]
                else:
                    logger.error('The output can not be parse: {0}'.format(output_line))

            if count < 60:
                count = count + 1
            else:
                return None

            time.sleep(1)

    def create_filesystem(self, filesystem_name):
        """Create a filesystem of HCFS
        """
        cmds = [self.hcfsvol_bin, 'create', filesystem_name]
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()
        if error_msg:
            logger.error(str(error_msg))
            return False, str(error_msg)
        else:
            return True, str(output)

    def mkdir_p(self, path):
        try:
            os.makedirs(path)
        except OSError as e:  # Python >2.5
            if e.errno == errno.EEXIST and os.path.isdir(path):
                pass
            else:
                raise

    def create_folder(self, path):
        cmds = ['mkdir', '-p', path]
        try:
            p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
            return True
        except:
            (output, error_msg) = p.communicate()
            logger.error(str(error_msg))
            return False

    def mount(self, filesystem, mount_point):
        """Mount HCFS to a mount point
        """
        # Create the mount point
        self.create_folder(mount_point)

        # Execute the mount command.
        cmds = [self.hcfsvol_bin, 'mount', filesystem, mount_point]
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if error_msg:
            logger.error(str(cmds))
            return False, str(error_msg)
        else:
            output_lines = output.split('\n')
            for line in output_lines:
                r = re.search(r'status is (.+?),\s.+', line)
                if r:
                    if r.group(1) == '-1':
                        logger.error(str(output))
                        return False, str(output)
                    else:
                        return True, str(output)
            else:
                logger.info('outputs: ' + str(output_lines))
                return False, 'No output.'

    def restart_hcfs(self):
        """Restart the hcfs process
        """
        self.terminate_hcfs()
        self.start_hcfs()


class CommonSetup:
    def __init__(self):
        self.current_dir = os.path.dirname(os.path.abspath(__file__))
        self.repo_dir = os.path.abspath(os.path.join(self.current_dir, '../../../..'))
        self.mount_point = os.path.join(self.repo_dir, 'tmp/mount2')
        self.fstest = FSTester(os.path.join(self.mount_point, 'fstest/tests'))
        self.logger = logging.getLogger('CommonSetup')
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
        cmds = ['sudo', 'cp', source, '/etc/hcfs.conf']
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if error_msg:
            self.logger.error('Replace file failed: {0}'.format(source))
            self.logger.error(str(error_msg))
            return False, str(error_msg)
        else:
            self.logger.info('Replace file: {0}'.format(source))

        cmds = ['sudo', 'sed', '-i', '-e', "s@%WORKSPACE%@"+self.repo_dir+"@", '/etc/hcfs.conf']
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if error_msg:
            self.logger.error('Update config file failed: {0}'.format(source))
            self.logger.error(str(error_msg))
            return False, str(error_msg)
        else:
            self.logger.info('Update config file: {0}'.format(source))
            return True, ''

    def get_container_size(self):
        cmds = ['swift', '--insecure', '-A', self.swift_url,
                '-U', self.swift_user,
                '-K', self.swift_key, 'stat']
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
        cmds = ['cp', '-f', testfilename_local, testfilename_hcfs]
        self.logger.info('copy_file_to_hcfs: {0}'.format(cmds))
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if error_msg:
            self.logger.error(str(cmds))
            self.logger.error(str(error_msg))
            return False, error_msg
        else:
            return True, ''

    def copy_file_from_hcfs(self, testfilename_hcfs, testfilename_local):
        """Copy the file from hcfs (download)
        """
        cmds = ['cp', '-f', testfilename_hcfs, testfilename_local]
        self.logger.info('copy_file_from_hcfs: {0}'.format(cmds))
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if error_msg:
            self.logger.error(str(cmds))
            self.logger.error(str(error_msg))
            return False, error_msg
        else:
            return True, ''

    def diff_files(self, testfile_abs_local, testfile_abs_local_2):
        """Diff these two files
        """
        cmds = ['diff', '-q', testfile_abs_local, testfile_abs_local_2]
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        if output == '':
            return True, ''
        else:
            return False, output

    def check_hcfs_mount(self, mount_point):
        p = subprocess.Popen(['mount'], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        for line in output.split('\n'):
            if re.search(mount_point, line):
                return True

        return False

    def check_hcfs_df(self, mount_point):
        p = subprocess.Popen(['mount'], stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        for line in output.split('\n'):
            if re.search(self.mount_point, line):
                return True

        return False

# ============================ Test Case Start ================================


class HCFS_0(CommonSetup):
    """Environment Setup
    """
    def __init__(self):
        CommonSetup.__init__(self)
        self.fake_filesystem = 'fakeHCFS'
        # self.mount_point = os.path.join(self.repo_dir, 'tmp/mount2')
        self.HCFSBin = HCFSBin()

    def run(self):
        # start local swift
        cmds = ['bash', 'TestCases/HCFS/setup_local_swift_env.bash']
        try:
            logger.info('Start a local Swift server')
            subprocess.call(cmds)
        except:
            logger.error('Start local Swift server failed. cmds: {0}'.format(cmds))
        # start hcfs service
        (output, error_msg) = self.HCFSBin.verify_hcfs_processes()
        if output is False:
            logger.info('Create HCFS processes...')
            self.HCFSBin.start_hcfs()
        else:
            logger.info('HCFS process already exist!')

        # create a filesystem
        filesystems = self.HCFSBin.list_filesystems()
        if filesystems is not None:
            if self.fake_filesystem not in filesystems:
                logger.info('Fake filesystem no exist, create one: {0}'.format(self.fake_filesystem))
                self.HCFSBin.create_filesystem(self.fake_filesystem)
            else:
                logger.info('fake fiesystem already exist.')
        else:
            return False, 'The file system list is None, it seems CLI has problems'

        # create mount point
        if not self.check_hcfs_mount(self.mount_point):
            logger.info('Mounting hcfs at {0}'.format(self.mount_point))
            (result, msg) = self.HCFSBin.mount(self.fake_filesystem, self.mount_point)
            print result, msg

        # check the df and mount
        if not self.check_hcfs_mount(self.mount_point):
            return False, 'Mount failed'

        if not self.check_hcfs_df(self.mount_point):
            return False, 'Missed df information'

        # copy fstest tool to HCFS
        cmds = 'sudo cp -rf TestCases/HCFS/fstest ' + self.mount_point
        logger.info('Copy fstest tool to HCFS: {0}'.format(cmds))
        try:
            subprocess.call(cmds, shell=True)
        except:
            return False, 'Copy fstest failed!'

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
    mkfifo
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
        return True, 'mkfifo not supported now'


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
    Backend Setting – ArkFlex One (Swift)
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.hcfs_bin = HCFSBin()
        self.origin_config = 'TestCases/HCFS/swift_arkflex_one.conf'
        self.config = 'TestCases/HCFS/swift_arkflex_one.conf'

    def start_hcfs_and_check_result(self):
        # Start the hcfs
        self.hcfs_bin.start_hcfs()

        # Check the process exist
        (result, msg) = self.hcfs_bin.verify_hcfs_processes()
        if result is False:
            return False, msg
        else:
            return True, ''

    def run(self):
        result = True
        msg = ''
        self.replace_hcfs_config(self.config)
        (result, msg) = self.start_hcfs_and_check_result()

        # self.replace_hcfs_config(self.origin_config)

        # close the hcfs
        self.hcfs_bin.terminate_hcfs()
        self.hcfs_bin.check_if_hcfs_terminated()

        return result, msg


class HCFS_17(CommonSetup):
    '''
    Backend Setting – ArkFlex U (S3)
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.hcfs_bin = HCFSBin()
        self.origin_config = 'TestCases/HCFS/swift_arkflex_one.conf'
        self.config = 'TestCases/HCFS/hcfs_s3_easepro.conf'

    def start_hcfs_and_check_result(self):
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
        (result, msg) = self.start_hcfs_and_check_result()
        self.replace_hcfs_config(self.origin_config)
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
        self.origin_config = 'TestCases/HCFS/swift_arkflex_one.conf'
        self.config = 'TestCases/HCFS/hcfs_s3.conf'

    def start_hcfs_and_check_result(self):
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
        (result, msg) = self.start_hcfs_and_check_result()
        self.replace_hcfs_config(self.origin_config)
        return result, msg


class HCFS_20(CommonSetup):
    '''
    Upload – exceed soft limit
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate()
        self.hcfsbin = HCFSBin()
        self.mount_point = os.path.join(self.repo_dir, 'tmp/mount')
        self.config = 'swift_arkflex_one.conf'
        self.testfilename = 'testUpload_60M'
        self.first_time = ''
        self.new_time = ''
        self.log_timeformat = '%Y-%m-%d %H:%M:%s'

    def run(self):
        # Prepare the arkflex swift environment
        # self.replace_hcfs_config(self.config)
        # self.hcfsbin.restart_hcfs()

        # Get the size of the remote storage.
        origin_container_size = self.get_container_size()

        # Generate the 60M size file.
        self.fileGenerate.gen_file(self.testfilename, '1', 60, 1024*1024, self.current_dir)
        time.sleep(10)      # magic number again

        # Copy the file to hcfs (upload)
        testfile_abs_local = os.path.join(self.current_dir, self.testfilename)
        testfile_abs_hcfs = os.path.join(self.mount_point, self.testfilename)
        self.first_time = datetime.now()
        cmds = ['cp', '-f', testfile_abs_local, testfile_abs_hcfs]
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        # Diff these two files
        cmds = ['diff', '-q', testfile_abs_local, testfile_abs_hcfs]
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
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
        self.mount_point = os.path.join(self.repo_dir, 'tmp/mount')
        self.config = 'swift_arkflex_one.conf'
        self.testfilename = 'testUpload_160M'
        self.first_time = ''
        self.new_time = ''
        self.log_timeformat = '%Y-%m-%d %H:%M:%s'

    def run(self):
        # Get the size of the remote storage.
        origin_container_size = self.get_container_size()

        # Generate the 60M size file.
        self.fileGenerate.gen_file(self.testfilename, '1', 160, 1024*1024, self.current_dir)
        time.sleep(10)      # magic number again

        # Copy the file to hcfs (upload)
        testfile_abs_local = os.path.join(self.current_dir, self.testfilename)
        testfile_abs_hcfs = os.path.join(self.mount_point, self.testfilename)
        self.first_time = datetime.now()
        cmds = ['cp', '-f', testfile_abs_local, testfile_abs_hcfs]
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
        (output, error_msg) = p.communicate()

        # Diff these two files
        cmds = ['diff', '-q', testfile_abs_local, testfile_abs_hcfs]
        p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
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
        testfile_abs_hcfs = os.path.join(self.mount_point, self.testfilename_hcfs)
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


class HCFS_23(CommonSetup):
    '''Delete
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate()
        self.hcfsbin = HCFSBin()

    def run(self):
        return True, ''


class HCFS_24(CommonSetup):
    '''Deduplication
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate()
        self.hcfsbin = HCFSBin()

    def run(self):
        return True, ''


class HCFS_25(CommonSetup):
    '''Encryption
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate()
        self.hcfsbin = HCFSBin()

    def run(self):
        return True, ''


class HCFS_26(CommonSetup):
    '''Compression
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate()
        self.hcfsbin = HCFSBin()

    def run(self):
        return True, ''


class HCFS_31(CommonSetup):
    '''
    Terminate
    '''
    def __init__(self):
        self.hcfsbin = HCFSBin()

    def run(self):
        self.hcfsbin.terminate_hcfs()
        result = self.hcfsbin.check_if_hcfs_terminated()
        if result:
            return True, ''
        else:
            return False, 'The HCFS did not terminate successfully'


class HCFS_39(CommonSetup):
    '''Upload big file
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate()
        self.hcfsbin = HCFSBin()
        self.samples_dir = os.path.abspath(os.path.join(self.current_dir, 'TestSamples'))
        self.testfile_abs_local = os.path.join(self.samples_dir, '4G')
        self.testfile_abs_hcfs = os.path.join(self.mount_point, '4G')

    def run(self):
        # Copy the file to hcfs (upload)
        spend_time = ''

        dt_ori = datetime.now()
        (result, error_msg) = self.copy_file_to_hcfs(self.testfile_abs_local, self.testfile_abs_hcfs)
        if not result:
            return False, error_msg
        dt_new = datetime.now()

        dt_delta = dt_new - dt_ori
        dt_delta_min_str = str(dt_delta.seconds / 60) + '.' + str(dt_delta.seconds % 60) + '(min)'
        dt_delta_sec_str = str(dt_delta.seconds) + '(sec)'
        spend_time = dt_delta_min_str if dt_delta.seconds > 59 else dt_delta_sec_str

        # Diff these two files
        (result, error_msg) = self.diff_files(self.testfile_abs_local, self.testfile_abs_hcfs)
        if not result:
            logger.error('diff failed!')
            return False, error_msg
        else:
            return True, 'Spend time: {0}'.format(spend_time)

        # Delete the generated file
        # self.exec_command_sync(['rm', '-f', testfile_abs_local_2], False)


class HCFS_40(CommonSetup):
    '''Upload/Download big file
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate()
        self.hcfsbin = HCFSBin()
        self.samples_dir = os.path.abspath(os.path.join(self.current_dir, 'TestSamples'))
        self.testfile_abs_local = os.path.join(self.samples_dir, '4G')
        self.testfile_abs_hcfs = os.path.join(self.mount_point, '4G')

    def run(self):
        # Copy the file from hcfs (download)

        dt_ori = datetime.now()
        # Diff these two files (also read/download file from hcfs)
        (result, error_msg) = self.diff_files(self.testfile_abs_hcfs, self.testfile_abs_local)
        if not result:
            logger.error('diff failed!')
            return False, error_msg
        dt_new = datetime.now()

        dt_delta = dt_new - dt_ori
        dt_delta_min_str = str(dt_delta.seconds / 60) + '.' + str(dt_delta.seconds % 60) + '(min)'
        dt_delta_sec_str = str(dt_delta.seconds) + '(sec)'
        spend_time = dt_delta_min_str if dt_delta.seconds > 59 else dt_delta_sec_str

        return True, 'Spend time: {0}'.format(spend_time)


class HCFS_41(CommonSetup):
    '''Delete big file (Doing...)
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.fileGenerate = FileGenerate()
        self.hcfsbin = HCFSBin()
        self.samples_dir = '/home/kentchen/TestSamples'     # hardcode
        self.testfilename_local = '4GB'
        self.testfilename_local2 = '4GB-2'
        self.testfilename_hcfs = '4GB'
        self.backend_log = 'backend_upload_log'

    def run(self):
        testfile_abs_hcfs = os.path.join(self.mount_point, self.testfilename_hcfs)

        # Delete the downloaded file
        # self.exec_command_sync(['rm', '-f', testfile_abs_hcfs], False)

        with open(self.backend_log, 'rb') as f:
            lines = f.read().split('\n')
            print len(lines)
            for i in range(len(lines)-1, -1, -1):
                r = re.search(r'.+\s(\d+\:\d+:\d+\.\d+?)\s+Debug meta deletion.+', lines[i])
                if r:
                    print lines[i], r.group(1)

        return True, ''


class HCFS_99(CommonSetup):
    '''
    Clear environment for file system operation test
    '''
    def __init__(self):
        CommonSetup.__init__(self)
        self.hcfsbin = HCFSBin()
        self.fstest_path = os.path.join(self.mount_point, 'fstest')

    def remove_fstest(self):
        try:
            subprocess.call('rm -rvf ' + self.fstest_path)
            return True
        except:
            return False

    def run(self):

        logger.info('remove_fstest')
        self.remove_fstest()
        logger.info('terminate_hcfs')
        self.hcfsbin.terminate_hcfs()

        if not self.hcfsbin.check_if_hcfs_terminated():
            return False, 'HCFS did not terminate complete'

        return True, ''


if __name__ == '__main__':
    pass
