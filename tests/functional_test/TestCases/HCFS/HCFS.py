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

class CommonSetup:
    def __init__(self):
        self.fstest = FSTester()

class HCFS_2(CommonSetup):
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
    