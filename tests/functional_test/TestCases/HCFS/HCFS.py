import os
import subprocess
from subprocess import PIPE

class FSTester:
    """
    """
    def __init__(self):
        self.path_of_scripts = 'TestCases/HCFS/fstest/tests'    # note here, it is hardcode
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

class CommonSetup:
    def __init__(self):
        self.fstest = FSTester()

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