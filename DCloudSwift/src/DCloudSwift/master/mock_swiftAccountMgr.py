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
import random

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
            stat = True if random.randrange(0, 2)==1 else False
            self.__account_dict[account] = {
                "description": "This is %s" % account,
                "user_number": self.__num_of_user,
                "account_enable": stat,
            }

        for j in range(self.__num_of_user):
            user = "User%d" % j
            stat = True if random.randrange(0, 2)==1 else False
            self.__user_dict[user] = {
                "description": "This is %s" % user,
                "user_enable": stat,
            }

    def __generate_random_password(self):
        '''
        Generate a random password of length 12. The characters of the random password are alphanumeric.

        @rtype:  string
        @return: a random password
        '''
        chars = string.letters + string.digits
        return "".join(random.choice(chars) for x in range(self.__random_password_size))

    def add_user(self, account, user, password="", description="", admin=False, reseller=False, retry=3):
        '''
        Add a user into an account, including the following steps::
            (1) Add a user.
            (2) Create the user's private container.
            (3) Create the user's metadata container.
            (4) Set ACL for the private container.
            (5) Set the metadata of the metadata container.

        @type  account: string
        @param account: the name of the account
        @type  user: string
        @param user: the name of the user
        @type  password: string
        @param password: the password to be set
        @type  description: string
        @param description: the description of the user
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

    def add_account(self, account, admin_user="", admin_password="", description="", retry=3):
        '''
        Add a new account, including the following steps::
            (1) Create the account and account administrator.
            (2) Create the private container for account administrator.
            (3) Create the metadata container for account administrator.
            (4) Set the metadata of the metadata container.

        @type  account: string
        @param account: the name of the account
        @type  admin_user: string
        @param admin_user: the name of account administrator
        @type  admin_password: string
        @param admin_password: the password of account administrator
        @type  description: string
        @param description: the description of the account
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

    def enable_user(self, account, user, retry=3):
        '''
        Enable the user to access backend Swift by restoring the original password kept in the metadata container.

        @type  account: string
        @param account: the account of the user
        @type  user: string
        @param user: the user to be enabled
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user's password is successfully restored to the original password kept in the
                metadata container, then Bool.val = True and Bool.msg = the standard output. Otherwise, Bool.val == False
                and Bool.msg indicates the error message.
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
        Disable the user to access the backend Swift by changing the password to a
        random string. The original password will be stored in the metadata container.

        @type  account: string
        @param account: the account of the user
        @type  user: string
        @param user: the user to be disabled
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user's password is successfully changed and the original password is
                stored in the metadata container, then Bool.val = True and Bool.msg = the standard output. Otherwise,
                Bool.val == False and Bool.msg indicates the error message.
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
        Enable the account by changing the passwords of all users from random password to original password saved
        in the metadata container. Note that after changing all users' passwords, the metadata must be updated.
        (Not finished yet)

        @type  account: string
        @param account: the name of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the account is enabled successfully, then Bool.val == True and msg == "".
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

    def disable_account(self, account, retry=3):
        '''
        Disable the account by changing the passwords of all users from original passwords to random passwords.
        The original password will be stored in the metadata. Note that after changing all users' passwords,
        the metadata must be updated.
        (Not finished yet)

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
        @param user: the user of the given account
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
        @param account: the account name of the user
        @type  user: string
        @param user: the user to get the password
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype: named tuple
        @return: a named tuple Bool(val, msg). If get the user's password successfully, then Bool.val == True, and
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

    def list_account(self, retry=3):
        '''
        List all existed accounts and related information.

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
        List all existed users and related information in the account.

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
    print "\nlist_account"
    pprint(SA.list_account().msg)
    print "\nlist_user"
    pprint(SA.list_user("account1").msg)
