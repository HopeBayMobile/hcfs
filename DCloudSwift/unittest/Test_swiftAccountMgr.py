# Created on 2012/08/29 by CW
# Unit test for swiftAccountMgr.py

import nose
import sys
import os
import random
import string
import subprocess
import json

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from master.swiftAccountMgr import SwiftAccountMgr


super_admin_password = "deltacloud"
auth_url = "127.0.0.1"
auth_port = "8080"


class CreateRandomString:
    def __init__(self, length, chars=string.letters + string.digits):
        self.__length  = length
        self.__chars = chars

    def generate(self):
        random_string = "".join(random.choice(self.__chars) for x in range(self.__length))
        return random_string


class Test_add_user:
    '''
    Test the function add_user() in swiftAccountMgr.py
    '''
    def setup(self):
        print "Start of unit test for function add_user() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__sa.add_account(self.__account)
        self.__sa.add_user(self.__account, self.__user, self.__password)

    def teardown(self):
        print "End of unit test for function add_user() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)

    def test_Integrity(self):
        '''
        Check the correctness of adding a user by add_user().
        '''

        # Check the existence of the user created by add_user().
        cmd = "swauth-list -A https://%s:%s/auth -K %s %s %s" % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "",\
                       "User %s:%s cannot be created by add_user()" % (self.__account, self.__user))

        # Check the existence of the metadata of the user created by add_user().
        cmd = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s download %s %s -o -"\
              % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "", "Failed to execute the command \'%s\': %s" % (cmd, stderrData))

        output = json.loads(stdoutData)
        mark = False

        for field, value in output.items():
            if field == self.__user:
                mark = True

        nose.tools.ok_(mark == True, "The metadat of user %s:%s does not exist." % (self.__account, self.__user))

        # Check the existence of the private container and gateway configuration container created by add_user().
        private_container = self.__user + "_private_container"
        gw_config_container = self.__user + "_gateway_config"

        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s list %s"\
               % (auth_url, auth_port, self.__account, self.__user, self.__password, private_container)

        po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "", "Container %s does not exist: %s" % (private_container, stderrData))

        cmd2 = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s list %s"\
               % (auth_url, auth_port, self.__account, self.__user, self.__password, gw_config_container)

        po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "", "Container %s does not exist: %s" % (gw_config_container, stderrData))


class Test_delete_user:
    '''
    Test the function delete_user() in swiftAccountMgr.py
    '''
    def setup(self):
        print "Start of unit test for function delete_user() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__sa.add_account(self.__account)
        self.__sa.add_user(self.__account, self.__user, self.__password)

        self.__sa.delete_user(self.__account, self.__user)

    def teardown(self):
        print "End of unit test for function delete_user() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)

    def test_Integrity(self):
        '''
        Check the correctness of deleting a user by delete_user().
        '''

        # Check whether the user is deleted by delete_user().
        cmd = "swauth-list -A https://%s:%s/auth -K %s %s" % (auth_url, auth_port, super_admin_password, self.__account)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "", "Failed to execute the command \'%s\': %s" % (cmd, stderrData))

        output = json.loads(stdoutData)
        mark = True

        for item in output["users"]:
            if item["name"] == self.__user:
                mark = False

        nose.tools.ok_(mark == True, "User %s:%s cannot be deleted by delete_user()" % (self.__account, self.__user))

        # Check whether the private container and gateway configuration container of the user are deleted by delete_user().
        private_container = self.__user + "_private_container"
        gw_config_container = self.__user + "_gateway_config"

        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s list %s"\
               % (auth_url, auth_port, self.__account, self.__user, self.__password, private_container)

        po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd1, stderrData))
        else:
            nose.tools.ok_(stderrData != "", "Container %s still exists: %s" % (private_container, stderrData))

        cmd2 = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s list %s"\
               % (auth_url, auth_port, self.__account, self.__user, self.__password, gw_config_container)

        po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd2, stderrData))
        else:
            nose.tools.ok_(stderrData != "", "Container %s still exists: %s" % (gw_config_container, stderrData))

        # Check whether the metadata of the user is removed by delete_user().
        cmd = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s download %s %s -o -"\
              % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "", "Failed to execute the command \'%s\': %s" % (cmd, stderrData))

        output = json.loads(stdoutData)
        mark = True

        for field, value in output.items():
            if field == self.__user:
                mark = False

        nose.tools.ok_(mark == True, "The metadata of user %s:%s does not exist." % (self.__account, self.__user))


class Test_add_account:
    '''
    Test for the function add_account() in swiftAccountMgr.py.
    '''
    def setup (self):
        print "Start of unit test for function add_account() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__sa.add_account(self.__account)

    def teardown(self):
        print "End of unit test for function add_account() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)

    def test_AccountExistence(self):
        '''
        Check the existence of the account created by add_account().
        '''
        cmd = "swauth-list -A https://%s:%s/auth -K %s %s" % (auth_url, auth_port, super_admin_password, self.__account)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "",\
                       "Account administrator %s:admin cannot be created by add_account()" % self.__account)

    def test_AdminExistence(self):
        '''
        Check the existence of account administrator created by add_account().
        '''

        # Check the existence of account administrator created by add_account()
        cmd = "swauth-list -A https://%s:%s/auth -K %s %s %s" % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "",\
                       "Account administrator %s:admin cannot be created by add_account()" % self.__account)

        # Check the existence of the metadata of account administrator created by add_account()
        cmd = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s download %s %s -o -"\
              % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "", "Failed to execute the command \'%s\': %s" % (cmd, stderrData))

        output = json.loads(stdoutData)
        mark = False

        for field, value in output.items():
            if field == "admin":
                mark = True

        nose.tools.ok_(mark == True, "The metadata of account administrator %s:admin does not exist." % self.__account)


class Test_delete_account:
    '''
    Test for the function delete_account() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function delete_account() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__sa.add_account(self.__account)
        self.__sa.delete_account(self.__account)

    def teardown(self):
        print "End of unit test for function delete_account() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)

    def test_AccountNonExistence(self):
        '''
        Check whether the account is deleted by delete_account().
        '''
        cmd = "swauth-list -A https://%s:%s/auth -K %s" % (auth_url, auth_port, super_admin_password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        nose.tools.ok_(po.returncode == 0 and stderrData == "", "Failed to execute the command \'%s\': %s" % (cmd, stderrData))

        output = json.loads(stdoutData)
        mark = True

        for item in output["accounts"]:
            if item["name"] == self.__account:
                mark = False

        nose.tools.ok_(mark == True, "Account %s cannot be deleted by delete_account()" % self.__account)


class Test_enable_user:
    '''
    Test for the function enable_user() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function enable_user() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__sa.add_account(self.__account)
        self.__sa.add_user(self.__account, self.__user, self.__password)

        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s download %s %s -o -"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        output = stdoutData.replace("auth", "disable")
        os.system("echo \'%s\' >> %s" % (output, self.__user))

        cmd2 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s upload %s %s"\
                % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        os.system(cmd2)
        os.system("rm %s" % self.__user)

        self.__sa.enable_user(self.__account, self.__user)

    def teardown(self):
        print "End of unit test for function enable_user() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)

    def test_Enabling(self):
        '''
        Check whether the user is enabled or not after executing enable_user().
        '''
        private_container = self.__user + "_private_container"
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s list %s"\
              % (auth_url, auth_port, self.__account, self.__user, self.__password, private_container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode == 0:
            nose.tools.ok_(stderrData == "", "User %s:%s cannot be enabled by enable_user()." % (self.__account, self.__user))
        else:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))


class Test_disable_user:
    '''
    Test for the function disable_user() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function disable_user() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__sa.add_account(self.__account)
        self.__sa.add_user(self.__account, self.__user, self.__password)

        self.__sa.disable_user(self.__account, self.__user)

    def teardown(self):
        print "End of unit test for function disable_user() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)

    def test_Disabling(self):
        '''
        Check whether the user is disabled or not after executing disable_user().
        '''
        private_container = self.__user + "_private_container"
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s list %s"\
              % (auth_url, auth_port, self.__account, self.__user, self.__password, private_container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))
        else:
            nose.tools.ok_(stderrData != "", "User %s:%s cannot be disabled by disable_user()." % (self.__account, self.__user))


class Test_enable_account:
    '''
    Test for the function enable_account() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function enable_account() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__sa.add_account(self.__account, "", self.__password)

        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s download %s .services -o -"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        output = stdoutData.replace("storage", "disable")
        os.system("echo \'%s\' >> .services" % output)

        cmd2 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s upload %s .services"\
                % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd2)
        os.system("rm .services")

        self.__sa.enable_account(self.__account)

    def teardown(self):
        print "End of unit test for function enable_account() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)

    def test_Enabling(self):
        '''
        Check whether the account is enabled or not after executing enable_account().
        '''
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:admin -K %s list"\
              % (auth_url, auth_port, self.__account, self.__password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode == 0:
            nose.tools.ok_(stderrData == "", "Account %s cannot be enabled by enable_account()." % self.__account)
        else:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))


class Test_disable_account:
    '''
    Test for the function disable_account() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function disable_account() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__sa.add_account(self.__account, "", self.__password)

        self.__sa.disable_account(self.__account)

    def teardown(self):
        print "End of unit test for function disable_account() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)

    def test_Disabling(self):
        '''
        Check whether the account is disabled or not after executing disable_account().
        '''
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:admin -K %s list"\
              % (auth_url, auth_port, self.__account, self.__password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))
        else:
            nose.tools.ok_(stderrData != "", "Account %s cannot be disabled by disable_account()." % self.__account)


class Test_change_password:
    '''
    Test for the function change_password() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function change_password() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__sa.add_account(self.__account)

        self.__sa.change_password(self.__account, "admin", self.__password)

    def teardown(self):
        print "End of unit test for function change_password() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)

    def test_ChangedPasswordValidation(self):
        '''
        Check the validation of the new password changed by change_password().
        '''
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:admin -K %s list"\
              % (auth_url, auth_port, self.__account, self.__password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))
        else:
            nose.tools.ok_(stderrData == "", "The new password of user %s:admin changed by change_password() is not valid." % self.__account)


class Test_get_user_password:
    '''
    Test for the function get_user_password() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function get_user_password() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__sa.add_account(self.__account)

        self.__output = self.__sa.get_user_password(self.__account, "admin")

    def teardown(self):
        print "End of unit test for function get_user_password() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)

    def test_Correctness(self):
        '''
        Check the correctness of the password returned by get_user_password().
        '''
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:admin -K %s list"\
              % (auth_url, auth_port, self.__account, self.__output.msg)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))
        else:
            nose.tools.ok_(stderrData == "", "The password of user %s:admin obtained by get_user_password() is not correct." % self.__account)


class Test_account_existence:
    '''
    Test for the function account_existence() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function account_existence() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__sa.add_account(self.__account)

    def teardown(self):
        print "End of unit test for function account_existence() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)

    def test_FalseNegative(self):
        '''
        False negative test for the valuse returned by account_existence().
        '''
        output = self.__sa.account_existence(self.__account)
        nose.tools.ok_(output.result == True, "Account %s has existed! The result returned by account_existence() is wrong." % self.__account)

    def test_FalsePositive(self):
        '''
        False positive test for the values returned by account_existence().
        '''
        random_account = CreateRandomString(8).generate()
        output = self.__sa.account_existence(random_account)
        nose.tools.ok_(output.result == False,\
                       "Account %s does not exist! The result returned by account_existence() is wrong." % random_account)


class Test_list_account:
    '''
    Test for the function list_account() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function list_account() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__quota = random.randrange(1000000000, 1000000000000)
        self.__description = CreateRandomString(20).generate()
        self.__sa.add_account(self.__account, "", "", self.__description, self.__quota)

        self.__output = self.__sa.list_account()

    def teardown(self):
        print "End of unit test for function list_account() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)

    def test_ContentIntegrity(self):
        '''
        Check the integrity of the contents returned by list_account().
        '''
        if self.__output.val == True:
            result = self.__output.msg.get(self.__account)
            if result == None:
                nose.tools.ok_(False, "The result returned by list_account() is not correct.")
            else:
                nose.tools.ok_(result.get("quota") == self.__quota, "The value of quota is not correct.")
                nose.tools.ok_(result.get("description") == self.__description, "The value of description is not correct.")
                nose.tools.ok_(result.get("account_enable") == True, "The value of account_enable is not correct.")
                nose.tools.ok_(result.get("usage") == 0, "The value of usage is not correct.")
                nose.tools.ok_(result.get("user_number") == 1, "The value of user_number is not correct.")
        else:
            nose.tools.ok_(False, "Failed to execute list_account(): %s" % self.__output.msg)


class Test_list_container:
    '''
    Test for the function list_container() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function list_container() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__container = CreateRandomString(12).generate()
        self.__sa.add_account(self.__account, "", self.__password)

        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:admin -K %s post %s"\
               % (auth_url, auth_port, self.__account, self.__password, self.__container)
        os.system(cmd)

        self.__output = self.__sa.list_container(self.__account, "admin")

    def teardown(self):
        print "End of unit test for function list_container() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd3 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)

    def test_ContentCorrectness(self):
        '''
        Check the correctness of the containers returned by list_container().
        '''
        if self.__output.val == True:
            nose.tools.ok_(self.__container in self.__output.msg, "The result returned by list_container() is not correct.")
        else:
            nose.tools.ok_(False, "Failed to execute list_container(): %s" % self.__output.msg)


class Test_list_user:
    '''
    Test for the function list_user() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function list_user() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__quota = random.randrange(1000000000, 1000000000000)
        self.__description = CreateRandomString(20).generate()
        self.__sa.add_account(self.__account)
        self.__sa.add_user(self.__account, self.__user, "", self.__description, self.__quota)

        self.__output = self.__sa.list_user(self.__account)

    def teardown(self):
        print "End of unit test for function list_user() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)

    def test_ContentIntegrity(self):
        ''' 
        Check the integrity of the contents returned by list_user().
        '''
        if self.__output.val == True:
            result = self.__output.msg.get(self.__user)
            if result == None:
                nose.tools.ok_(False, "The result returned by list_user() is not correct.")
            else:
                nose.tools.ok_(result.get("quota") == self.__quota, "The value of quota is not correct.")
                nose.tools.ok_(result.get("description") == self.__description, "The value of description is not correct.")
                nose.tools.ok_(result.get("user_enable") == True, "The value of user_enable is not correct.")
                nose.tools.ok_(result.get("usage") == 0, "The value of usage is not correct.")
        else:
            nose.tools.ok_(False, "Failed to execute list_user(): %s" % self.__output.msg)


class Test_user_existence:
    '''
    Test for the function user_existence() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function user_existence() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__sa.add_account(self.__account)
        self.__sa.add_user(self.__account, self.__user)

    def teardown(self):
        print "End of unit test for function user_existence() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)

    def test_FalseNegative(self):
        '''
        False negative test for the valuse returned by user_existence().
        '''
        output = self.__sa.user_existence(self.__account, self.__user)
        nose.tools.ok_(output.result == True,\
                       "User %s:%s has existed! The result returned by user_existence() is wrong." % (self.__account, self.__user))

    def test_FalsePositive(self):
        '''
        False positive test for the values returned by user_existence().
        '''
        random_account = CreateRandomString(8).generate()
        random_user = CreateRandomString(8).generate()
        output = self.__sa.user_existence(random_account, random_user)
        nose.tools.ok_(output.result == False,\
                       "User %s:%s does not exist! The result returned by account_existence() is wrong." % (random_account, random_user))


class Test_assign_read_acl:
    '''
    Test for the function assign_read_acl() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function assign_read_acl() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__container = CreateRandomString(8).generate()
        self.__sa.add_account(self.__account, "", self.__password)
        self.__sa.add_user(self.__account, self.__user, self.__password)

        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:admin -K %s post %s"\
              % (auth_url, auth_port, self.__account, self.__password, self.__container)
        os.system(cmd)

        self.__sa.assign_read_acl(self.__account, self.__container, self.__user, "admin")

    def teardown(self):
        print "End of unit test for function assign_read_acl() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)

    def test_CheckReadACL(self):
        '''
        Check the correctness of the read ACL assigned by assign_read_acl().
        '''
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s list %s"\
              % (auth_url, auth_port, self.__account, self.__user, self.__password, self.__container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))
        else:
            nose.tools.ok_(stderrData == "",\
                           "Failed to assign read ACL by assign_read_acl(): User %s:%s cannot access container %s."\
                           % (self.__account, self.__user, self.__container))


class Test_assign_write_acl:
    '''
    Test for the function assign_write_acl() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function assign_write_acl() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__container = CreateRandomString(8).generate()
        self.__sa.add_account(self.__account, "", self.__password)
        self.__sa.add_user(self.__account, self.__user, self.__password)
        self.__file = CreateRandomString(10).generate()

        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:admin -K %s post %s"\
              % (auth_url, auth_port, self.__account, self.__password, self.__container)
        os.system(cmd)

        os.system("touch %s" % self.__file)

        self.__sa.assign_write_acl(self.__account, self.__container, self.__user, "admin")

    def teardown(self):
        print "End of unit test for function assign_read_acl() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        cmd5 = "rm %s" % self.__file
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)
        os.system(cmd5)

    def test_CheckWriteACL(self):
        '''
        Check the correctness of the read ACL assigned by assign_read_acl().
        '''
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s upload %s %s"\
              % (auth_url, auth_port, self.__account, self.__user, self.__password, self.__container, self.__file)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))
        else:
            nose.tools.ok_(stderrData == "",\
                           "Failed to assign write ACL by assign_read_acl(): User %s:%s cannot write container %s."\
                           % (self.__account, self.__user, self.__container))


class Test_remove_read_acl:
    '''
    Test for the function remove_read_acl() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function remove_read_acl() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__container = CreateRandomString(8).generate()
        self.__sa.add_account(self.__account, "", self.__password)
        self.__sa.add_user(self.__account, self.__user, self.__password)

        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:admin -K %s post %s -r \'%s:%s\'"\
              % (auth_url, auth_port, self.__account, self.__password, self.__container, self.__account, self.__user)
        os.system(cmd)

        self.__sa.remove_read_acl(self.__account, self.__container, self.__user, "admin")

    def teardown(self):
        print "End of unit test for function assign_read_acl() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)

    def test_CheckReadACL(self):
        '''
        Check the correctness of the read ACL assigned by assign_read_acl().
        '''
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s list %s"\
              % (auth_url, auth_port, self.__account, self.__user, self.__password, self.__container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))
        else:
            nose.tools.ok_(stderrData != "",\
                           "Failed to remove read ACL by remove_read_acl(): User %s:%s still can access container %s."\
                           % (self.__account, self.__user, self.__container))


class Test_remove_write_acl:
    '''
    Test for the function remove_write_acl() in swiftAccountMgr.py.
    '''
    def setup(self):
        print "Start of unit test for function remove_write_acl() in swiftAccountMgr.py\n"
        self.__sa = SwiftAccountMgr()
        self.__account = CreateRandomString(8).generate()
        self.__user = CreateRandomString(8).generate()
        self.__password = CreateRandomString(12).generate()
        self.__container = CreateRandomString(8).generate()
        self.__sa.add_account(self.__account, "", self.__password)
        self.__sa.add_user(self.__account, self.__user, self.__password)
        self.__file = CreateRandomString(10).generate()

        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:admin -K %s post %s -r \'%s:%s\' -w \'%s:%s\'"\
              % (auth_url, auth_port, self.__account, self.__password, self.__container,\
                 self.__account, self.__user, self.__account, self.__user)
        os.system(cmd)

        os.system("touch %s" % self.__file)

        self.__sa.remove_write_acl(self.__account, self.__container, self.__user, "admin")

    def teardown(self):
        print "End of unit test for function assign_read_acl() in swiftAccountMgr.py\n"
        cmd1 = "swift -A https://%s:%s/auth/v1.0 -U .super_admin:.super_admin -K %s delete %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, ".metadata")
        cmd2 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, self.__user)
        cmd3 = "swauth-delete-user -A https://%s:%s/auth -K %s %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account, "admin")
        cmd4 = "swauth-delete-account -A https://%s:%s/auth -K %s %s"\
               % (auth_url, auth_port, super_admin_password, self.__account)
        cmd5 = "rm %s" % self.__file
        os.system(cmd1)
        os.system(cmd2)
        os.system(cmd3)
        os.system(cmd4)
        os.system(cmd5)

    def test_CheckReadACL(self):
        '''
        Check the correctness of the read ACL assigned by assign_read_acl().
        '''
        cmd = "swift -A https://%s:%s/auth/v1.0 -U %s:%s -K %s upload %s %s"\
              % (auth_url, auth_port, self.__account, self.__user, self.__password, self.__container, self.__file)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            nose.tools.ok_(False, "Failed to execute the command %s: %s" % (cmd, stderrData))
        else:
            nose.tools.ok_(stderrData != "",\
                           "Failed to remove write ACL by remove_write_acl(): User %s:%s still can write container %s."\
                           % (self.__account, self.__user, self.__container))


if __name__ == "__main__":
    pass
