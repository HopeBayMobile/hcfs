import os
import sys
import socket
import posixfile
import time
import json
import subprocess
import threading
import datetime
import logging
import pickle
import collections
import random
import string
import sqlite3
import uuid
from pprint import pprint

lock = threading.Lock()


class SwiftAccountMgr:
    def __init__(self, conf=""):
        self.__random_password_size = 12
        self.__num_of_account = random.randrange(3, 20)
        self.__num_of_user = random.randrange(1, 20)
        self.__account_dict = {}
        self.__user_dict = {}

        for i in range(self.__num_of_account):
            account = "Account%d" % i
            stat = True if random.randrange(0, 2) == 1 else False
            account_quota = random.randrange(1, 1000000000)
            account_usage = random.randrange(0, 1000000000)

            self.__account_dict[account] = {
                "description": "This is %s" % account,
                "user_number": self.__num_of_user,
                "account_enable": stat,
                "quota": account_quota,
                "usage": account_usage,
            }

        for j in range(self.__num_of_user):
            user = "User%d" % j
            stat = True if random.randrange(0, 2) == 1 else False
            user_quota = random.randrange(1, 1000000000)
            user_usage = random.randrange(0, 1000000000)

            self.__user_dict[user] = {
                "description": "This is %s" % user,
                "user_enable": stat,
                "quota": user_quota,
                "usage": user_usage,
            }

    def __generate_random_password(self):
        '''
        Generate a random password of length 12. The characters of the random password are alphanumeric.

        @rtype:  string
        @return: a random password
        '''
        chars = string.letters + string.digits
        return "".join(random.choice(chars) for x in range(self.__random_password_size))

    def add_user(self, account, user, password="", description="no description", quota=0, admin=False, reseller=False, retry=3):
        '''
        Add a user into the account, including the following steps::
            (1) Add a user.
            (2) Create the user's private container and configuration container.
            (3) Create the user's metadata stored in super_admin account.
            (4) Set ACL for the user's private container.

        @type  account: string
        @param account: the name of the account
        @type  user: string
        @param user: the name of the user
        @type  password: string
        @param password: the password to be set
        @type  description: string
        @param description: the description of the user
        @type  quota: integer
        @param quota: the quota of the user in the number of bytes
        @type  admin: boolean
        @param admin: admin or not
        @type  reseller: boolean
        @param reseller: reseller or not
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user is successfully added to backend Swift, then Bool.val == True
                and msg records the standard output. Otherwise, val == False and msg records the error message.
        '''
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        elif user == "" or user == None:
            msg = "User is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def delete_user(self, account, user, retry=3):
        '''
        Delete the user from backend Swift. The user's data will be destroyed.
        The deletion includes the following steps::
            (1) Delete a user.
            (2) Remove the user's metadata from super_admin account.
            (3) Delete the user's private container and configuration container.

        @type  account: string
        @param account: the name of the account
        @type  user: string
        @param user: the name of the user
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user is successfully deleted from backend Swift, then Bool.val == True
                and msg records the standard output. Otherwise, val == False and msg records the error message.
        '''
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        elif user == "" or user == None:
            msg = "User is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def add_account(self, account, admin_user="", admin_password="", description="no description", quota=500000000000, retry=3):
        '''
        Add a new account, including the following steps::
            (1) Create the account and account administrator.
            (2) Create account administrator's metadata stored in super_admin account.

        @type  account: string
        @param account: the name of the account
        @type  admin_user: string
        @param admin_user: the name of account administrator
        @type  admin_password: string
        @param admin_password: the password of account administrator
        @type  description: string
        @param description: the description of the account
        @type  quota: integer
        @param quota: the quota of the account in the number of bytes
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). When the account is successfully created, Bool.val == True.
                Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def delete_account(self, account, retry=3):
        '''
        Remove all users from the account and delete the account from backend Swift.
        All data will be destroyed.

        @type  account: string
        @param account: the name of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the account is successfully deleted, then Bool.val ==
                True. Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def enable_user(self, account, user, retry=3):
        '''
        Enable the user by modifying the file "<user name>" of the container "<account name>" in super_admin account.

        @type  account: string
        @param account: the account having the user
        @type  user: string
        @param user: the user to be enabled
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user is successfully enabled, then Bool.val = True and
                Bool.msg = the standard output. Otherwise, Bool.val == False and Bool.msg indicates the error message.
        '''
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def disable_user(self, account, user, retry=3):
        '''
        Disable the user by modifying the file "<user name>" of the container "<account name>" in super_admin account.

        @type  account: string
        @param account: the account of the user
        @type  user: string
        @param user: the user to be disabled
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user is successfully disabled, then Bool.val = True and
                Bool.msg = the standard output. Otherwise, Bool.val == False and Bool.msg indicates the error message.
        '''
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        elif user == "" or user == None:
            msg = "User is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def enable_account(self, account, retry=3):
        '''
        Enable the account by modifying the file ".services" of the container "<account name>" in super_admin account.

        @type  account: string
        @param account: the account to be enabled
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the account is enabled successfully, then Bool.val == True and msg == "".
                Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def disable_account(self, account, retry=3):
        '''
        Disable the account by modifying the file ".services" of the container "<account name>" in super_admin account.

        @type  account: string
        @param account: the name of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the account is disabled successfully, then Bool.val == True and msg == "".
                Otherwise, Bool.val == False and Bool.msg records the error message including the black list.
        '''
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def change_password(self, account, user, newPassword="", oldPassword="", retry=3):
        '''
        Change the password of a Swift user in the account.

        @type  account: string
        @param account: the name of the account
        @type  user: string
        @param user: the user to change the password
        @type  newPassword: string
        @param newPassword: the new password of the user
        @type  oldPassword: string
        @param oldPassword: the original password of the user
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the password is changed successfully, then Bool.val == True and msg == "".
                Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        elif user == "" or user == None:
            msg = "User is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def get_user_password(self, account, user, retry=3):
        '''
        Return the user's password.

        @type  account: string
        @param account: the account having the user
        @type  user: string
        @param user: the user to get the password
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype: named tuple
        @return: a named tuple Bool(val, msg). If the user's password is successfully got, then Bool.val == True, and
                Bool.msg == password. Otherwise, Bool.val == False, and Bool.msg records the error message.
        '''
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid"
        elif user == "" or user == None:
            msg = "User is not valid!"
        else:
            val = True
            msg = "ThisIsPasswd"

        return Bool(val, msg)

    def set_account_quota(self, account, quota, retry=3):
        '''
        Set the quota of the account by modifying the file ".metadata" of the contianer "<account name>" in super_admin account.

        @type  account: string
        @param account: the account to set the quota
        @type  quota: integer
        @param quota: the quota of the account in number of bytes
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the account's quota is successfully set, then Bool.val = True and
                Bool.msg = the standard output. Otherwise, Bool.val == False and Bool.msg indicates the error message.
        '''
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid"
        elif str(quota).isdigit() != True:
            msg = "Quota is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def set_user_quota(self, account, user, quota, retry=3):
        '''
        Set the quota of the user by modifying the file ".metadata" of the contianer "<account name>" in super_admin account.

        @type  account: string
        @param account: the account having the user
        @type  user: string
        @param user: the user to set the quota
        @type  quota: integer
        @param quota: the quota of the user in number of bytes
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user's quota is successfully set, then Bool.val = True and
                Bool.msg = the standard output. Otherwise, Bool.val == False and Bool.msg indicates the error message.
        '''
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid"
        elif user == "" or account == None:
            msg = "User is not valid"
        elif str(quota).isdigit() != True:
            msg = "Quota is not valid!"
        else:
            val = True

        return Bool(val, msg)

    def list_account(self, retry=3):
        '''
        List all existed accounts and related information, including::
            (1) the number of users
            (2) the description of the account
            (3) the quota of the account
            (4) the account is enabled or not
            (5) the usage of the account

        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the account information is listed successfully, then Bool.val == True
                and Bool.msg is a dictoinary recording all existed accounts and related information. Otherwise,
                Bool.val == False and Bool.msg records the error message.
        '''
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        val = True
        msg = self.__account_dict

        return Bool(val, msg)

    def list_user(self, account, retry=3):
        '''
        List all existed users and related information in the account, including::
            (1) the description of the user
            (2) the quota of the user
            (3) the user is enabled or not
            (4) the usage of the user

        @type  account: string
        @param account: the account name of the user
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the user list is successfully got, then Bool.val == True and
                Bool.msg is a dictionary recording all existed users and related information. Otherwise,
                Bool.val == False and Bool.msg records the error message.
        '''
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        val = True
        msg = self.__user_dict

        return Bool(val, msg)

    def obtain_account_info(self, account, retry=3):
        '''
        Obtain the related information of the account, including::
            (1) the number of users
            (2) the description of the account
            (3) the quota of the account
            (4) the account is enabled or not
            (5) the usage of the account

        @type  account: string
        @param account: the account to obtain the information
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the account information is obtained successfully, then Bool.val == True
                and Bool.msg is a dictoinary recording all related information of the account. Otherwise,
                Bool.val == False and Bool.msg records the error message.
        '''
        val = False
        msg = {}
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        else:
            val = True

            msg = {
                "description": "This is account %s" % account,
                "account_enable": True,
                "quota": random.randrange(1, 1000000000),
                "usage": random.randrange(1, 500000000),
                "user_number": random.randrange(1, 1000),
            }

        return Bool(val, msg)

    def obtain_user_info(self, account, user, account_enable=True, retry=3):
        '''
        Obtain the related information of the user in the account, including::
            (1) the description of the user
            (2) the quota of the user
            (3) the usgae of the user
            (4) the user is enabled or not

        @type  account: string
        @param account: the account having the user
        @type  user: string
        @param user: the user to obtain the related information
        @type  account_enable: boolean
        @param account_enable: the accout is enabled or not
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the information of the user is successfully got, then Bool.val == True
                and Bool.msg is a dictionary recording the related information of the user. Otherwise,
                Bool.val == False and Bool.msg records the error message.
        '''
        val = False
        msg = {}
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        elif user == "" or user == None:
            msg = "User is not valid!"
        else:
            val = True

            msg = {
                "description": "This is user %s:%s" % (account, user),
                "user_enable": True,
                "quota": random.randrange(1, 10000000),
                "usage": random.randrange(1, 1000000),
            }

        return Bool(val, msg)

    def modify_user_description(self, account, user, description, retry=3):
        '''
        Modify the description of the user by modifying the file ".metadata" of the
        contianer "<account name>" in super_admin account.

        @type  account: string
        @param account: the account having the user
        @type  user: string
        @param user: the user to modify the description
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the description of the user is successfully modified, then Bool.val == True
                and Bool.msg == "". Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        val = True
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if account == "" or account == None:
            msg = "Account is not valid!"
        elif user == "" or user == None:
            msg = "User is not valid!"
        elif description == "" or description == None:
            msg = "Description can not be empty!"
        else:
            val = True

        return Bool(val, msg)


if __name__ == '__main__':
    SA = SwiftAccountMgr()
    print "\nadd_user"
    print SA.add_user("account1", "user1")
    print "\nadd_account"
    print SA.add_account("account1")
    print "\nenable_user"
    print SA.enable_user("account1", "user1")
    print "\ndisable_user"
    print SA.disable_user("account1", "user1")
    print "\nenable_account"
    print SA.enable_account("account1")
    print "\ndisable_account"
    print SA.disable_account("account1")
    print "\nchange_password"
    print SA.change_password("account1", "user1")
    print "\nget_user_password"
    print SA.get_user_password("account1", "user1")
    print "\nset_account_quota"
    print SA.set_account_quota("account1", 100)
    print "\nset_user_quota"
    print SA.set_user_quota("account1", "user1", 100)
    print "\nlist_account"
    pprint(SA.list_account().msg)
    print "\nlist_user"
    pprint(SA.list_user("account1").msg)
    print "\nobtain_account_info"
    pprint(SA.obtain_account_info("account1").msg)
    print "\nobtain_user_info"
    pprint(SA.obtain_user_info("account1", "user1").msg)
    print "\nmodify_user_description"
    print SA.modify_user_description("account1", "user1", "This is a test!")
