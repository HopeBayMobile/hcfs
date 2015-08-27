import os
import subprocess
from subprocess import PIPE

class FSTester:
    """
    """
    def __init__(self):
        self.path_of_scripts = 'fstest/tests'
        self.harness = 'prove'
        
    def execute(self, test_type, script_filename):
        """Execute the fstest

        :param test_type: The type of test command ex. chmod
        :param script_filename: The test script file name.
        """
        target_script = os.path.join(os.path.join(self.path_of_scripts, test_type), script_filename)
        cmds = [self.harness, '-r', target_script]
        try:
            p = subprocess.Popen(cmds, stdout=PIPE, stderr=PIPE)
            (output, error_msg) = p.communicate()
            result = True
        except OSError as e:
            result = False
            output = e
        except ValueError as e:
            result = False
            output = e

        return result, output

class HCFSOperate:
    def __init__(self):
        self.conf = {
            'METAPATH': '/home/kentchen/testHCFS/metastorage'
            'BLOCKPATH': '/home/kentchen/testHCFS/blockstorage'
            'CACHE_SOFT_LIMIT': '53687091'
            'CACHE_HARD_LIMIT': '107374182'
            'CACHE_DELTA': '10485760'
            'MAX_BLOCK_SIZE': '1048576'
            'CURRENT_BACKEND': 's3'
            'SWIFT_ACCOUNT': 'kentchen'
            'SWIFT_USER': 'kentchen'
            'SWIFT_PASS': '0qrrbOCHoNUM'
            'SWIFT_URL': '10.10.99.11:8080'
            'SWIFT_CONTAINER': 'kentchen_private_container'
            'SWIFT_PROTOCOL': 'https'
            'S3_ACCESS': 'AKIAJ3HMUZ2RY3FUSJMA'
            'S3_SECRET': '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'
            'S3_URL': 's3.amazonaws.com'
            'S3_BUCKET': 'kentchen'
            'S3_PROTOCOL': 'https'
            'LOG_LEVEL': '10'
        }

    def generate_conf_file(self):
        """Need root privileage
        """

class CommonSetup:
    def __init__(self):
        self.fstest = FSTester()

class HCFS_0(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''



class HCFS_3(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        CommonSetup.__init__(self)
        self.script_filename = '01.t'
        self.type = 'chflag'
        
    def run(self):
        (result, output) = self.fstest.execute(self.type, slef.script_filename)

        print result

        return False, ''


class HCFS_4(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''

class HCFS_5(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''

class HCFS_6(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''

class HCFS_7(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''

class HCFS_8(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''

class HCFS_9(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''

class HCFS_10(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''

class HCFS_11(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''

class HCFS_12(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''


class HCFS_13(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''


class HCFS_14(CommonSetup):
    '''
    write doc here
    '''    
    def __init__(self):
        pass
        
    def run(self):
        return False, ''

