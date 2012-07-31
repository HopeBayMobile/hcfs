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

#Self defined packages
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/" % BASEDIR)

from util import util
from util import threadpool
from util.SwiftCfg import SwiftCfg
from util.util import GlobalVar
from util.database import AccountDatabaseBroker
from util.database import DatabaseConnectionError

lock = threading.Lock()


class InconsistentDatabaseError(Exception):
    pass


class SwiftAccountMgr:
    def __init__(self, conf=GlobalVar.ORI_SWIFTCONF):
        logger = util.getLogger(name="SwiftAccountMgr.__init__")

        if os.path.isfile(conf):
            cmd = "cp %s %s" % (conf, GlobalVar.SWIFTCONF)
            os.system(cmd)
        else:
            msg = "Confing %s does not exist" % conf
            print >> sys.stderr, msg
            logger.warn(msg)

        if not os.path.isfile(GlobalVar.SWIFTCONF):
            msg = "Config %s does not exist" % GlobalVar.SWIFTCONF
            print >> sys.stderr, msg
            logger.error(msg)
            sys.exit(1)

        self.__deltaDir = GlobalVar.DELTADIR
        self.__swiftDir = self.__deltaDir + "/swift"

        self.__SC = SwiftCfg(GlobalVar.SWIFTCONF)
        self.__kwparams = self.__SC.getKwparams()
        self.__password = self.__kwparams['password']

        self.__admin_default_name = "admin"
        self.__random_password_size = 12
        #chars = string.letters + string.digits
        #self.__random_password = ''.join(random.choice(chars) for x in range(8))
        self.__private_container_suffix = "_private_container"
        self.__shared_container_suffix = "_shared_container"

        if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
            os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

    def __generate_random_password(self):
        '''
        Generate a random password.

        @rtype:  string
        @return: a random password
        '''
        logger = util.getLogger(name="__generate_random_password")
        chars = string.letters + string.digits
        return "".join(random.choice(chars) for x in range(self.__random_password_size))

    def __functionBroker(self, proxy_ip_list, retry, fn, **kwargs):
        '''
        Repeat at most retry times:
        1. Execute the private function fn with a randomly chosen
        proxy node and kwargs as input.
        2. Break if fn retrun True

        @type  proxy_ip_list: string
        @param proxy_ip_list: ip list of proxy nodes
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @type  fn: string
        @param fn: private function to call
        @param kwargs: keyword arguments to fn
        @rtype:  tuple
        @return: a tuple (val, msg). If fn is executed successfully, then val == True and msg records
            the standard output. Otherwise, val == False and msg records the error message.
        '''
        #TODO: need to modify proxy_ip_list due to the security issues
        val = False
        msg = ""

        for t in range(retry):
            ip = random.choice(proxy_ip_list)

            try:
                output = fn(ip, **kwargs)
                if output.val == True:
                    val = True
                    msg = output.msg
                    break
                else:
                    errMsg = "Failed to run %s thru %s for %s" % (fn.__name__, ip, output.msg)
                    msg = msg + '\n' + errMsg
            except util.TimeoutError:
                errMsg = "Failed to run %s thru %s in time" % (fn.__name__, ip)
                msg = msg + '\n' + errMsg

        return (val, msg)

    @util.timeout(300)
    def __add_user(self, proxyIp, account, user, password, admin=False, reseller=False):
        logger = util.getLogger(name="__add_user")

        url = "https://%s:8080/auth/" % proxyIp
        msg = "Failed to add user: "
        val = False

        admin_opt = "-a " if admin else ""
        reseller_opt = "-r " if reseller  else ""
        optStr = admin_opt + reseller_opt

        cmd = "swauth-add-user -K %s -A %s %s %s %s %s" % (self.__password, url, optStr, account, user, password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        Bool = collections.namedtuple("Bool", "val msg")
        return Bool(val, msg)

    def add_user(self, account, user, password="", description="", admin=False, reseller=False, retry=3):
        '''
        Add a user into a given account, including the following steps::
            (1) Add a user
            (2) Create a private container for the user by the admin user
            (3) Create a metadata container account:user in super_admin account
            (4) Set ACL and metdata for the private container and the metadata container, respectively

        @type  account: string
        @param account: the name of the given account
        @type  user: string
        @param user: the name of the given user
        @type  password: string
        @param password: the password to be set
        @type  description: string
        @param description: the description of the user
        @type  admin: boolean
        @param admin: admin or not
        @type  reseller: boolean
        @param reseller: reseller or not
        @type  retry: integer
        @param retry: the maximum number of times to retry
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user is successfully added to both the database and backend swift then
            Bool.val == True and msg records the standard output. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="add_user")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        private_container = user + self.__private_container_suffix
        metadata_container = account + ":" + user

        #TODO: need to check the characters of the password
        if password == "":
            password = self.__generate_random_password()

        metadata_content = {
                "Account-Enable": True,
                "User-Enable": True,
                "Password": password,
                "Quota": 0,
                "Description": description,
                "Usage": 0,
        }

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        check_account_existence = self.account_existence(account)

        if user == self.__admin_default_name:
            if check_account_existence.result == True:
                val = False
                msg = "Account %s has existed!" % account
                return Bool(val, msg)
        else:
            if check_account_existence.val == False:
                val = False
                msg = check_account_existence.msg
                return Bool(val, msg)
            elif check_account_existence.result == False:
                val = False
                msg = "Account %s does not exist!" % account
                return Bool(val, msg)

            user_existence_output = self.user_existence(account, user)

            if user_existence_output.val == False:
                val = False
                msg = user_existence_output.msg
                return Bool(val, msg)
            elif user_existence_output.result == True:
                val = False
                msg = "User %s:%s has existed!" % (account, user)
                return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__add_user, account=account,\
                                           user=user, password=password, admin=admin, reseller=reseller)

        if val == False:
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=metadata_container, metadata_content=metadata_content)

        if val == False:
            #TODO: need to rollback
            logger.error(msg)
            return Bool(val, msg)

        get_admin_password_output = self.get_user_password(account, self.__admin_default_name)

        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        write_acl = {
            "Read": account + ":" + user,
            "Write": account + ":" + user,
        } 

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=account, admin_user=self.__admin_default_name, admin_password=admin_password,\
                                           container=private_container, metadata_content=write_acl)

        if val == False:
            #TODO: need to rollback
            logger.error(msg)
        else:
            val = True
            logger.info(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __delete_user(self, proxyIp, account, user):
        logger = util.getLogger(name="__delete_user")

        url = "https://%s:8080/auth/" % proxyIp
        msg = "Failed to delete user: "
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swauth-delete-user -K %s -A %s %s %s" % (self.__password, url, account, user)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def delete_user(self, account, user, retry=3):
        '''
        Delete the user from backend swift. All data of the user will be destroyed.

        @type  account: string
        @param account: the name of the given account
        @type  user: string
        @param user: the name of the given user
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user is successfully deleted to both the database and backend swift then
            Bool.val == True and msg records the standard output. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="delete_user")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")
        metadata_container = account + ":" + user
        private_container = user + self.__private_container_suffix
        admin_password = ""

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        user_existence_output = self.user_existence(account, user)

        if user_existence_output.val == False:
            val = False
            msg = user_existence_output.msg
            return Bool(val, msg)
        elif user_existence_output.result == False:
            val = False
            msg = "User %s:%s does not exist!" % (account, user)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_user, account=account, user=user)

        if val == False:
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_container, account=".super_admin",\
                                           container=metadata_container, admin_user=".super_admin", admin_password=self.__password)

        if val == False:
            # TODO: need to rollback
            msg = "Failed to delete the metadata container: " + msg
            logger.error(msg)
            return Bool(val, msg)

        get_admin_password_output = self.get_user_password(account, self.__admin_default_name)

        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_container, account=account,\
                                           container=private_container, admin_user=self.__admin_default_name, admin_password=admin_password)

        if val == False:
            # TODO: need to rollback
            msg = "Failed to delete the private container: " + msg
            logger.error(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __add_account(self, proxyIp, account):
        logger = util.getLogger(name="__add_account")

        url = "https://%s:8080/auth/" % proxyIp
        msg = "Failed to add account: "
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swauth-add-account -K %s -A %s %s" % (self.__password, url, account)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def add_account(self, account, admin_user="", admin_password="", description="", retry=3):
        '''
        Add a new account, including the following things::
            (1) Create the account and the admin user
            (2) Create the private container for the admin user
            (3) Create the metadata container account:admin_user in super_admin account
            (4) Set metadata for the metadata container

        @type  account: string
        @param account: the name of the given account
        @type  admin_user: string
        @param admin_user: the name of admin_user
        @type  admin_password: string
        @param admin_password: the password of the admin user
        @type  description: string
        @param description: the description of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). When the account is successfully created, Bool.val == True.
            Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="add_account")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        #TODO: need to check the characters of the name of admin_user
        if admin_user == "":
            admin_user = self.__admin_default_name

        #TODO: need to check the characters of the password
        if admin_password == "":
            admin_password = self.__generate_random_password()

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        check_account_existence = self.account_existence(account)

        if check_account_existence.val == False:
            val = False
            msg = check_account_existence.msg
            return Bool(val, msg)
        elif check_account_existence.result == True:
            val = False
            msg = "Account %s has existed!" % account
            return Bool(val, msg)

        add_user_output = self.add_user(account=account, user=self.__admin_default_name, password="",\
                                        description=description, admin=True)

        if add_user_output.val == False:
            val = False
            msg = "Failed to add the admin user: " + add_user_output.msg
            logger.error(msg)
        else:
            val = True
            msg = add_user_output.msg
            logger.info(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __delete_account(self, proxyIp, account):
        logger = util.getLogger(name="__delete_account")

        url = "https://%s:8080/auth/" % proxyIp
        msg = "Failed to delete account: "
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swauth-delete-account -K %s -A %s %s" % (self.__password, url, account)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def delete_account(self, account, retry=3):
        '''
        Remove all users from the account and delete the account from backend Swift.
        All data will be destroyed.

        @type  account: string
        @param account: the name of the given account
        @type  retry: integer
        @param retry: the maximum number of times to retry
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the account is successfully deleted, then Bool.val ==
            True. Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="delete_account")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        msg = ""
        val = False
        black_list = []
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        check_account_existence = self.account_existence(account)

        if check_account_existence.val == False:
            val = False
            msg = check_account_existence.msg
            return Bool(val, msg)
        elif check_account_existence.result == False:
            val = False
            msg = "Account %s does not exist!" % account
            return Bool(val, msg)

        list_user_output = self.list_user(account)

        if list_user_output.val == False:
            val = False
            msg = "Failed to list all users: " + list_user_output.msg
            return Bool(val, msg)
        else:
            logger.info(list_user_output.msg)
            user_list = list_user_output.msg

        for user in user_list:
            metadata_container = account + ":" + user
            (val1, msg1) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_user, account=account, user=user)
            (val2, msg2) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_container,\
                                                 account=".super_admin", container=metadata_container, admin_user=".super_admin",\
                                                 admin_password=self.__password)

            if val1 == False or val2 == False:
                black_list.append("%s:%s" % (account, user))

        if len(black_list) != 0:
            val = False
            msg = black_list
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_account, account=account)

        if val == False:
            logger.error(msg)

        return Bool(val, msg)

    def enable_user(self, account, user, retry=3):
        '''
        Enable the user to access backend Swift by restoring the original
        password kept in the metadata container.

        @type  account: string
        @param account: the account of the user
        @type  user: string
        @param user: the user to be enabled
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user's password is successfully restored to the original password kept in the
            metadata container, then Bool.val = True and Bool.msg = the standard output. Otherwise, Bool.val == False
            and Bool.msg indicates the error message.
        '''
        logger = util.getLogger(name="enable_user")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        ori_user_password = ""
        actual_user_password = ""
        container_metadata = {}
        container = account + ":" + user

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, user)

        if get_user_password_output.val == False:
            val = False
            msg = get_user_password_output.msg
            return Bool(val, msg)
        else:
            actual_user_password = get_user_password_output.msg

        #get_admin_password_output = self.get_user_password(account, admin_user)
        #if get_admin_password_output.val == False:
        #    val = False
        #    msg = get_admin_password_output.msg
        #    return Bool(val, msg)
        #else:
        #    admin_password = get_admin_password_output.msg

        # TODO: check whehter the container is associated with the user
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                           account=".super_admin", container=container, admin_user=".super_admin",\
                                           admin_password=self.__password)

        if val == False:
            msg = "Failed to get the metadata of the container %s" % container + msg
            return Bool(val, msg)
        else:
            container_metadata = msg

        if container_metadata["Account-Enable"] == False:
            val = False
            msg = "Failed to enable user %s: account %s does not enable" % (user, account)
            return Bool(val, msg)
        elif container_metadata["User-Enable"] == True:
            val = True
            msg = "The user %s has enabled" % user
            return Bool(val, msg)
        elif container_metadata["Password"] == actual_user_password:
            val = True
            msg = "The user %s has enabled" % user
            return Bool(val, msg)
        else:
            ori_user_password = container_metadata["Password"]
            container_metadata["User-Enable"] = True

        change_password_output = self.change_password(account, user, ori_user_password)

        if change_password_output.val == False:
            val = False
            msg = change_password_output.msg
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=".super_admin", container=container, admin_user=".super_admin",\
                                           admin_password=self.__password, metadata_content=container_metadata)

        if val == False:
            # TODO: need to rollback
            logger.error(msg)

        return Bool(val, msg)

    def disable_user(self, account, user, retry=3):
        '''
        Disable the user to access the backend Swift by changing the password to a
        random string. The original password will be stored in the metadata container.

        @type  account: string
        @param account: the account of the user
        @type  container: string
        @param container: the container for the user
        @type  user: string
        @param user: the user to be disabled
        @type  admin_user: string
        @param admin_user: the admin user of the container
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user's password is successfully changed and the original password is
            stored in the metadata container, then Bool.val = True and Bool.msg = the standard output. Otherwise,
            Bool.val == False and Bool.msg indicates the error message.
        '''
        logger = util.getLogger(name="disable_user")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        new_user_password = str(uuid.uuid4())
        actual_user_password = ""
        container = account + ":" + user

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, user)

        if get_user_password_output.val == False:
            val = False
            msg = get_user_password_output.msg
            return Bool(val, msg)
        else:
            actual_user_password = get_user_password_output.msg

        #get_admin_password_output = self.get_user_password(account, admin_user)
        #if get_admin_password_output.val == False:
        #    val = False
        #    msg = get_admin_password_output.msg
        #    return Bool(val, msg)
        #else:
        #    admin_password = get_admin_password_output.msg

        # TODO: check whether the container is associated with the user

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                           account=".super_admin", container=container, admin_user=".super_admin",\
                                           admin_password=self.__password)

        if val == False:
            msg = "Failed to get the metadata of the container %s" % container + msg
            return Bool(val, msg)
        else:
            container_metadata = msg

        if container_metadata["Account-Enable"] == False:
            val = False
            msg = "Failed to disable user %s: account %s does not enable" % (user, account)
            return Bool(val, msg)
        elif container_metadata["User-Enable"] == False:
            val = True
            msg = "The user %s has disabled" % user
            return Bool(val, msg)
        elif container_metadata["Password"] != actual_user_password:
            val = True
            msg = "The user %s has disabled" % user
            return Bool(val, msg)

        change_password_output = self.change_password(account, user, new_user_password)

        if change_password_output.val == False:
            val = False
            msg = change_password_output.msg
            return Bool(val, msg)

        container_metadata = {"User-Enable": False,
                              "Password": actual_user_password}

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=".super_admin", container=container, admin_user=".super_admin",\
                                           admin_password=self.__password, metadata_content=container_metadata)

        return Bool(val, msg)

    def enable_account(self, account, retry=3):
        '''
        Enable the account by changing the passwords of all users from random
        password to original password saved in the metadata. Note that after
        changing all users' passwords, the metadata must be updated. (Not finished yet)

        @type  account: string
        @param account: the name of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the account is enabled
            successfully, then Bool.val == True and msg == "". Otherwise,
            Bool.val == False and Bool.msg records the error message
            including the black list.
        '''
        logger = util.getLogger(name="enable_account")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        user_info = {}
        user_list = []
        user_metadata = {}
        black_list = []
        admin_user = ""  # to be defined
        admin_password = ""
        user_container = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_user_info,\
                                           account=account)

        if val == False:
            return Bool(val, msg)

        try:
            user_info = json.loads(msg)
            msg = ""

        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            val = False
            return Bool(val, msg)

        if len(user_info["users"]) == 0:
            val = False
            msg = "There are no users in the account %s!" % account
            return Bool(val, msg)

        for item in user_info["users"]:
            user_list.append(item["name"])

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False or\
           get_admin_password_output.msg == "":

            val = False

            msg = "Failed to get the password of the admin user: %s"\
                  % get_admin_password_output.msg

            return Bool(val, msg)

        else:
            admin_password = get_admin_password_output.msg

        for user in user_list:
            # TODO: use thread pool to speed up
            user_container = ""  # to be defined
            user_metadata = {"Account-Enable": True}

            get_user_password_output = self.get_user_password(account, user)
            if get_user_password_output.val == False or\
               get_user_password_output.msg == "":

                black_list.append("Failed to get the password of %s: %s"\
                                  % (user, get_user_password_output.msg))

                continue

            else:
                ori_password = get_user_password_output.msg

                (val, msg) = self.__functionBroker(\
                                proxy_ip_list=proxy_ip_list, retry=retry,\
                                fn=self.__get_container_metadata,\
                                account=account, container=user_container,\
                                admin_user=admin_user,\
                                admin_password=admin_password)

                if val == False:
                    black_list.append(\
                        "Failed to get the original password of %s: %s"\
                        % (user, msg))

                    continue

                elif msg["User-Enable"] == False:
                    new_password = ori_password
                    user_metadata["Password"] = msg["Password"]

                else:
                    new_password = msg["Password"]

                change_password_output = self.change_password(account, user,\
                                                              ori_password,\
                                                              new_password)

            if change_password_output.val == False:
                black_list.append("Failed to change the password of %s: %s"\
                                  % (user, change_password_output.msg))
                continue

            if user == admin_user:
                admin_password = new_password

            (val, msg) = self.__functionBroker(\
                            proxy_ip_list=proxy_ip_list, retry=retry,\
                            fn=self.__set_container_metadata, account=account,\
                            container=user_container, admin_user=admin_user,\
                            admin_password=admin_password,\
                            metadata_content=user_metadata)

            if val == False:
                black_list.append("Failed to update the metadta of %s: %s"\
                                  % (user, msg))

        if len(black_list) != 0:
            val = False
            msg = black_list

        else:
            val = True
            msg = ""

        return Bool(val, msg)

    def disable_account(self, account, retry=3):
        '''
        Disable the account by changing the passwords of all users from
        original passwords to random passwords. The original password will
        be stored in the metadata. Note that after changing all users'
        passwords, the metadata must be updated. (Not finished yet)

        @type  account: string
        @param account: the name of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the account is disabled
            successfully, then Bool.val == True and msg == "". Otherwise,
            Bool.val == False and Bool.msg records the error message
            including the black list.
        '''
        logger = util.getLogger(name="diable_account")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        user_info = {}
        user_list = []
        user_metadata = {}
        black_list = []
        admin_user = ""  # to be defined
        admin_password = ""
        user_container = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_user_info,\
                                           account=account)

        if val == False:
            return Bool(val, msg)

        try:
            user_info = json.loads(msg)
            msg = ""

        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            val = False
            return Bool(val, msg)

        if len(user_info["users"]) == 0:
            val = False
            msg = "There are no users in the account %s!" % account
            return Bool(val, msg)

        for item in user_info["users"]:
            user_list.append(item["name"])

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False or\
           get_admin_password_output.msg == "":

            val = False

            msg = "Failed to get the password of the admin user: %s"\
                  % get_admin_password_output.msg

            return Bool(val, msg)

        else:
            admin_password = get_admin_password_output.msg

        for user in user_list:
            # TODO: use thread pool to speed up
            user_container = ""  # to be defined

            (val, msg) = self.__functionBroker(\
                            proxy_ip_list=proxy_ip_list, retry=retry,\
                            fn=self.__get_container_metadata,\
                            account=account, container=user_container,\
                            admin_user=admin_user,\
                            admin_password=admin_password)

            if val == False:
                black_list.append("Failed to get the metadata of %s: %s"\
                                  % (user, msg))
                continue

            else:
                ori_user_metadata = msg

            get_user_password_output = self.get_user_password(account, user)
            if get_user_password_output.val == False or\
               get_user_password_output.msg == "":

                black_list.append("Failed to get the password of %s: %s"\
                                  % (user, get_user_password_output.msg))

                continue

            else:
                ori_password = get_user_password_output.msg
                new_password = str(uuid.uuid4())
                change_password_output = self.change_password(account, user,\
                ori_password, new_password)

            if change_password_output.val == False:
                black_list.append("Failed to change the password of %s: %s"\
                                  % (user, change_password_output.msg))

                continue

            if user == admin_user:
                admin_password = new_password

            # BUG: check whether User-Enable is False
            if ori_user_metadata["User-Enable"] == False:
                user_metadata = {"Account-Enable": False,
                                 "Password": ori_user_metadata["Password"]}

            else:
                user_metadata = {"Account-Enable": False,
                                 "Password": ori_password}

            (val, msg) = self.__functionBroker(\
                            proxy_ip_list=proxy_ip_list, retry=retry,\
                            fn=self.__set_container_metadata,\
                            account=account, container=user_container,\
                            admin_user=admin_user,\
                            admin_password=admin_password,\
                            metadata_content=user_metadata)

            if val == False:
                black_list.append("Failed to update the metadta of %s: %s"\
                                  % (user, msg))

        if len(black_list) != 0:
            val = False
            msg = black_list

        else:
            val = True
            msg = ""

        return Bool(val, msg)

    @util.timeout(300)
    def __change_password(self, proxyIp, account, user, newPassword, admin=True, reseller=False):
        '''
        Change the password of the given user in Swift.

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the name of the given account
        @type  user: string
        @param user: the name of the given user
        @type  newPassword: string
        @param newPassword: the new password to be set
        @type  admin: boolean
        @param admin: admin or not
        @type  reseller: boolean
        @param reseller: reseller or not
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operation is successfully done, then val == True and msg records the
            standard output. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__change_password")

        url = "https://%s:8080/auth/" % proxyIp
        msg = "Failed to change the password of %s: " % user
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        admin_opt = "-a " if admin else ""
        reseller_opt = "-r " if reseller  else ""
        optStr = admin_opt + reseller_opt

        #TODO: Must fix the format of password to use special character
        cmd = "swauth-add-user -K %s -A %s %s %s %s %s" % (self.__password, url, optStr, account, user, newPassword)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def change_password(self, account, user, newPassword="", oldPassword="", retry=3):
        '''
        Change the password of a Swift user in a given account.

        @type  account: string
        @param account: the name of the account
        @type  user: string
        @param user: the user of the given account
        @type  oldPassword: string
        @param oldPassword: the original password of the user
        @type  newPassword: string
        @param newPassword: the new password of the user
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the password is changed successfully, then Bool.val == True and msg == "".
            Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="change_password")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        user_detail = {}
        admin = False
        reseller = False
        ori_password = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        # TODO: check the format of the new password
        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        user_existence_output = self.user_existence(account, user)

        if user_existence_output.val == False:
            val = False
            msg = user_existence_output.msg
            return Bool(val, msg)
        elif user_existence_output.result == False:
            val = False
            msg = "User %s does not exist!" % user
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_user_detail,\
                                           account=account, user=user)

        if val == False:
            return Bool(val, msg)

        try:
            user_detail = json.loads(msg)
            msg = ""
        except Exception as e:
            val = False
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            return Bool(val, msg)

        for item in user_detail["groups"]:
            if item["name"] == ".admin":
                admin = True
            if item["name"] == ".reseller_admin":
                reseller = True

        #ori_password = user_detail["auth"].split(":")[1]
        #if oldPassword != ori_password:
        #    val = False
        #    msg = "Authentication failed! The old password is not correct!"
        #    return Bool(val, msg)

        if newPassword == "":
            newPassword = self.__generate_random_password()

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__change_password, account=account,\
                                           user=user, newPassword=newPassword, admin=admin, reseller=reseller)

        if val == False:
            return Bool(val, msg)

        #admin_user = self.__admin_default_name
        #admin_password = ""
        user_metadata_container = user + self.__private_container_suffix
        container_metadata = {"Password": newPassword}

        #get_user_password_output = self.get_user_password(account, admin_user)

        #if get_user_password_output.val == True:
        #    admin_password = get_user_password_output.msg
        #else:
        #    val = False
        #    msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
        #    logger.error(msg)
        #    return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=".super_admin", container=user_metadata_container, admin_user=".super_admin",\
                                           admin_password=self.__password, metadata_content=container_metadata)

        if val == False:
            # TODO: need to rollback
            logger.error(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __get_account_usage(self, proxyIp, account, user):
        '''
        Return the statement of the given user in the give account. (Not finished yet)

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the name of the given account
        @type  user: string
        @param user: the name of the given user
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operatoin is successfully done, then val == True and msg records the
            information of the given user. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_account_usage")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        password = self.get_user_password(account, user).msg

        cmd = "swift -A %s -U %s:%s -K %s stat"\
              % (url, account, user, password)

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,\
                              stderr=subprocess.PIPE)

        (stdoutData, stderrData) = po.communicate()

        msg = ""
        val = False

        if po.returncode != 0:
            logger.error(stderrData)
            msg = stderrData
            val = False
        else:
            logger.info(stdoutData)
            msg = stdoutData
            val = True

        Bool = collections.namedtuple("Bool", "val msg")
        return Bool(val, msg)

    def get_account_usage(self, account, user, retry=3):
        '''
        get account usage from the backend swift (Not finished yet)

        @type  account: string
        @param account: the account name of the given user
        @type  user: string
        @param user: the user to be checked
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If get the account usage
            successfully, then Bool.val == True, and Bool.msg == "".
            Otherwise, Bool.val == False, and Bool.msg records the
            error message.
        '''
        logger = util.getLogger(name="get_account_usage")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)

        msg = ""
        val = False
        user_detail = {}
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_account_usage,\
                                           account=account, user=user)

        return Bool(val, msg)

    @util.timeout(300)
    def __get_user_detail(self, proxyIp, account, user):
        '''
        Return the information of the given user in the give account.

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the name of the given account
        @type  user: string
        @param user: the name of the given user
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operatoin is successfully done, then val == True and msg records the
            information of the given user. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_user_detail")

        url = "https://%s:8080/auth/" % proxyIp
        msg = "Failed to get the user detail: "
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swauth-list -K %s -A %s %s %s" % (self.__password, url, account, user)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def get_user_password(self, account, user, retry=3):
        '''
        Return user password.

        @type  account: string
        @param account: the account name of the given user
        @type  user: string
        @param user: the user to be checked
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype: named tuple
        @return: a named tuple Bool(val, msg). If get the user's password successfully, then Bool.val == True, and
            Bool.msg == password. Otherwise, Bool.val == False, and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="get_user_password")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        user_detail = {}
        user_password = ""
        password = ""
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_user_detail,\
                                           account=account, user=user)

        if val == False:
            return Bool(val, msg)

        try:
            user_detail = json.loads(msg)
            val = True
            msg = ""
        except Exception as e:
            val = False
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            return Bool(val, msg)

        user_password = user_detail["auth"]

        if user_password is None:
            msg = "Failed to get password of user %s:%s" % (account, user)
        else:
            password = user_password.split(":")
            if password != -1:
                msg = password[-1]

        return Bool(val, msg)

    def is_admin(self, account, user, retry=3):
        '''
        Return whether the given user is admin. (Not finished yet)

        @type  account: string
        @param account: the account name of the given user
        @type  user: string
        @param user: the user to be checked
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(result, val, msg). If the user
            is admin, then Bool.result == True, Bool.val == True,
            and Bool.msg == "". If the user is not admin, then
            Bool.result == False, Bool.val == True, and Bool.msg == "".
            Otherwise, Bool.result == False, Bool.val == False, and
            Bool.msg records the error message.
        '''
        logger = util.getLogger(name="is_admin")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        user_detail = {}
        result = False
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "result val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(result, val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(result, val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_user_detail,\
                                           account=account, user=user)

        if val == False:
            result = False
            return Bool(result, val, msg)

        try:
            user_detail = json.loads(msg)
            val = True
            msg = ""

        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            result = False
            val = False
            return Bool(result, val, msg)

        for item in user_detail["groups"]:
            if item["name"] == ".admin":
                result = True

        return Bool(result, val, msg)

    def is_reseller(self, account, user, retry=3):
        '''
        Return whether the given user is reseller admin. (Not finished yet)

        @type  account: string
        @param account: the account name of the given user
        @type  user: string
        @param user: the user to be checked
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(result, val, msg). If the user
            is reseller admin, then Bool.result == True, Bool.val == True,
            and Bool.msg == "". If the user is not reseller admin, then
            Bool.result == False, Bool.val == True, and Bool.msg == "".
            Otherwise, Bool.result == False, Bool.val == False, and
            Bool.msg records the error message.
        '''
        logger = util.getLogger(name="is_reseller")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        user_detail = {}
        result = False
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "result val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(result, val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(result, val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_user_detail,\
                                           account=account, user=user)

        if val == False:
            result = False
            return Bool(result, val, msg)

        try:
            user_detail = json.loads(msg)
            val = True
            msg = ""

        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            result = False
            val = False
            return Bool(result, val, msg)

        for item in user_detail["groups"]:
            if item["name"] == ".reseller_admin":
                result = True

        return Bool(result, val, msg)

    def set_account_quota(self, account, admin_container,\
                          admin_user, quota, retry=3):
        '''
        Set the quota of the given account by updating the metadata
        in the container for the admin user of the given account. (Not finished yet)

        @type  account: string
        @param account: the account to be set quota
        @type  admin_container: string
        @param admin_container: the container for the admin user
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  quota: integer
        @param quota: quota of the account (bytes)
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the account's quota is successfully
            set, then Bool.val = True and Bool.msg = the standard output.
            Otherwise, Bool.val == False and Bool.msg indicates the error
            message.
        '''
        logger = util.getLogger(name="set_account_quota")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        admin_password = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        #TODO: check whehter the container admin_container is associated
        #with admin_user

        #TODO: check whether the quota is a valid number

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        container_metadata = {"Quota": quota}

        (val, msg) = self.__functionBroker(\
                        proxy_ip_list=proxy_ip_list, retry=retry,\
                        fn=self.__set_container_metadata, account=account,\
                        container=admin_container, admin_user=admin_user,\
                        admin_password=admin_password,\
                        metadata_content=container_metadata)

        return Bool(val, msg)

    def get_account_quota(self, account, admin_container, admin_user, retry=3):
        '''
        Get the quota of the given account by reading the metadata in
        the container for the admin user of the given account. (Not finished yet)

        @type  account: string
        @param account: the account to be set quota
        @type  admin_container: string
        @param admin_container: the container for the admin user
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the account's quota is successfully
            got, then Bool.val = True and Bool.msg = the quota of the given
            account. Otherwise, Bool.val == False and Bool.msg indicates the
            error message.
        '''
        logger = util.getLogger(name="get_account_quota")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        admin_password = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        #TODO: check whehter the container admin_container is associated
        #with admin_user

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        (val, msg) = self.__functionBroker(\
                        proxy_ip_list=proxy_ip_list, retry=retry,\
                        fn=self.__get_container_metadata, account=account,\
                        container=admin_container, admin_user=admin_user,\
                        admin_password=admin_password)

        if val == False:
            return Bool(val, msg)

        elif msg["Quota"].isdigit():
            msg = int(msg["Quota"])

        else:
            val = False
            msg = "The value of the quota in the metadata is not a number."

        return Bool(val, msg)

    def set_user_quota(self, account, container, user,\
                       admin_user, quota, retry=3):
        '''
        Set the quota of the given user by updating the metadata in
        the container for the user. (Not finished yet)

        @type  account: string
        @param account: the account of the given user
        @type  container: string
        @param container: the container for the given user
        @type  user: string
        @param user: the user to be set quota
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  quota: integer
        @param quota: quota of the account (bytes)
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user's quota is successfully
            set, then Bool.val = True and Bool.msg = the standard output.
            Otherwise, Bool.val == False and Bool.msg indicates the
            error message.
        '''
        logger = util.getLogger(name="set_user_quota")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        admin_password = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        #TODO: check whehter the container is associated with the given user
        #TODO: check whether the quota is a valid number

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        container_metadata = {"Quota": quota}

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, metadata_content=container_metadata)

        return Bool(val, msg)

    def get_user_quota(self, account, container, user, admin_user, retry=3):
        '''
        Get the quota of the given user by reading the metadata in
        the container for the user. (Not finished yet)

        @type  account: string
        @param account: the account to be set quota
        @type  container: string
        @param container: the container for the given user
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user's quota is successfully
            got, then Bool.val = True and Bool.msg = the quota of the given
            user. Otherwise, Bool.val == False and Bool.msg indicates the
            error message.
        '''
        logger = util.getLogger(name="get_user_quota")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        admin_password = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        #TODO: check whehter the container is associated with the given user

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            return Bool(val, msg)
        elif msg["Quota"].isdigit():
            msg = int(msg["Quota"])
        else:
            val = False
            msg = "The value of the quota in the metadata is not a number."

        return Bool(val, msg)

    @util.timeout(300)
    def __get_account_info(self, proxyIp):
        '''
        Get the account information of Swift.

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operation is successfully done, then val == True and msg will
            record the information. Otherwise, val == False and msg will record the error message.
        '''
        logger = util.getLogger(name="__get_account_info")

        url = "https://%s:8080/auth/" % proxyIp
        msg = "Failed to get the account information: "
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swauth-list -K %s -A %s" % (self.__password, url)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def account_existence(self, account, retry=3):
        '''
        Check whether the given account exists.

        @type  account: string
        @param account: an account name to be queried
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(result, val, msg). If the account exists, then Bool.result == True, Bool.val == True,
            and Bool.msg == "". If the account does not exist, then Bool.result == False, Bool.val == True, and Bool.msg == "".
            Otherwise, Bool.result == False, Bool.val == False, and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="account_existence")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        account_info = {}
        result = False
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "result val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(result, val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(result, val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_account_info)

        if val == False:
            result = False
            return Bool(result, val, msg)

        try:
            account_info = json.loads(msg)
            val = True
            msg = ""
        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            result = False
            val = False
            return Bool(result, val, msg)

        for item in account_info["accounts"]:
            if item["name"] == account and ":" not in item["name"]:
            # Metadata container must be excluded
                result = True

        return Bool(result, val, msg)

    def list_account(self, retry=3):
        '''
        List all the existed accounts.

        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(result, val, msg). If the account information is listed successfully, then Bool.val == True
            and Bool.msg == account list. Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="list_account")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        account_info = {}
        account_list = []
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_account_info)

        if val == False:
            return Bool(val, msg)

        try:
            account_info = json.loads(msg)
            val = True
            msg = ""
        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            val = False
            return Bool(val, msg)

        for item in account_info["accounts"]:
            if ":" not in item["name"]:
            # Metadata container must be removed.
                account_list.append(item["name"])

        msg = account_list

        return Bool(val, msg)

    @util.timeout(300)
    def __get_container_info(self, proxyIp, account, admin_user, admin_password):
        '''
        Return the container information of a given account.

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the account to be queried
        @type  admin_user: string
        @param admin_user: the admin user of the given account
        @type  admin_password: string
        @param admin_password: the password of the admin user
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operation is successfully done, then val == True and msg records
            the container information. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_container_info")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = "Failed to get the container information: "
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s list" % (url, account, admin_user, admin_password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def list_container(self, account, admin_user, retry=3):
        '''
        List all containers of a given account.

        @type  account: string
        @param account: the account name of the given user
        @type  admin_user: string
        @param admin_user: the admin user of the given account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the container list is got successfully, then Bool.val == True
            and Bool.msg == user list. Otherwise, Bool.val == False, and Bool.msg records the error message.
        '''

        logger = util.getLogger(name="list_container")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        account_existence_output = self.account_existence(account)

        if account_existence_output.val == False:
            val = False
            msg = list_account_output.msg
            return Bool(val, msg)
        elif account_existence_output.result == False:
            val = False
            msg = "Account %s does not exist" % account
            return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_info,\
                                           account=account, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            return Bool(val, msg)
        elif msg == "":
            val = True
            msg = []
            return Bool(val, msg)
        else:
            val = True
            msg = msg.split("\n")
            msg.remove("")
            return Bool(val, msg)

    @util.timeout(300)
    def __get_user_info(self, proxyIp, account):
        '''
        Return the user information of a given account.

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the account to be queried
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operation is successfully done, then val == True and msg records the user
            information. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_user_info")

        url = "https://%s:8080/auth/" % proxyIp
        msg = "Failed to get the user information: "
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swauth-list -K %s -A %s %s" % (self.__password, url, account)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def list_user(self, account, retry=3):
        '''
        List all the existed accounts.

        @type  account: string
        @param account: the account name of the given user
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the user list is successfully got, then Bool.val == True and
            Bool.msg == user list. Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="list_user")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        user_info = {}
        user_list = []
        val = False
        msg = ""

        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        account_existence_output = self.account_existence(account)

        if account_existence_output.val == False:
            val = False
            msg = list_account_output.msg
            return Bool(val, msg)
        elif account_existence_output.result == False:
            val = False
            msg = "Account %s does not exist" % account
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_user_info, account=account)

        if val == False:
            return Bool(val, msg)
        else:
            try:
                user_info = json.loads(msg)
                val = True
                msg = ""
            except Exception as e:
                val = False
                msg = "Failed to load the json string: %s" % str(e)
                logger.error(msg)
                return Bool(val, msg)

        for item in user_info["users"]:
            user_list.append(item["name"])

        msg = user_list

        return Bool(val, msg)

    def user_existence(self, account, user, retry=3):
        '''
        Check whether the given user exists in the account.

        @type  account: string
        @param account: the account name of the given user
        @type  user: string
        @param user: the user to be checked
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(result, val, msg). If the user exists, then Bool.result == True, Bool.val == True,
            and Bool.msg == "". If the user does not exist, then Bool.result == False, Bool.val == True, and Bool.msg == "".
            Otherwise, Bool.result == False, Bool.val == False, and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="user_existence")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        user_info = {}
        result = False
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "result val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(result, val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(result, val, msg)

        account_existence_output = self.account_existence(account)

        if account_existence_output.val == False:
            result = False
            val = False
            msg = list_account_output.msg
            return Bool(result, val, msg)
        elif account_existence_output.result == False:
            result = False
            val = False
            msg = "Account %s does not exist" % account
            return Bool(result, val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_user_info, account=account)

        if val == False:
            result = False
            return Bool(result, val, msg)

        try:
            user_info = json.loads(msg)
            val = True
            msg = ""
        except Exception as e:
            result = False
            val = False
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            return Bool(result, val, msg)

        for item in user_info["users"]:
            if item["name"] == user:
                result = True

        return Bool(result, val, msg)

    def assign_read_acl(self, account, container, user, admin_user, retry=3):
        '''
        Assign the user to the read acl of the container.

        @type  account: string
        @param account: the account of the user and admin_user
        @type  container: string
        @param container: the container to assign the read acl
        @type  user: string
        @param user: the user to add into the read acl
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the read acl is successfully assigned, then val == True and msg == "".
            Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="assign_read_acl")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            return Bool(val, msg)
        else:
            if container not in msg:
                val = False
                msg = "Container %s does not exist!" % container
                return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to get metadata of the container %s: %s" % (container, msg)
            logger.error(msg)
            return Bool(val, msg)
        else:
            ori_read_acl = msg["Read"]

            if "%s:%s" % (account, user) in ori_read_acl:
                val = True
                msg = ""
                return Bool(val, msg)
            else:
                msg["Read"] = ori_read_acl + "," + "%s:%s" % (account, user)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, metadata_content=msg)

        if val == False:
            msg = "Failed to set metadata of the container %s: %s" % (container, msg)
            logger.error(msg)

        return Bool(val, msg)

    def assign_write_acl(self, account, container, user, admin_user, retry=3):
        '''
        Assign the user to the write acl of the container.

        @type  account: string
        @param account: the account of the user and admin_user
        @type  container: string
        @param container: the container to assign the write acl
        @type  user: string
        @param user: the user to add into the write acl
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the write acl is successfully assigned, then val == True and msg == "".
            Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="assign_write_acl")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            return Bool(val, msg)
        else:
            if container not in msg:
                val = False
                msg = "Container %s does not exist!" % container
                return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to get metadata of the container %s: %s" % (container, msg)
            logger.error(msg)
            return Bool(val, msg)
        else:
            ori_read_acl = msg["Read"]
            ori_write_acl = msg["Write"]

            if "%s:%s" % (account, user) in ori_write_acl and "%s:%s" % (account, user) in ori_read_acl:
                val = True
                msg = ""
                return Bool(val, msg)

            if "%s:%s" % (account, user) not in ori_read_acl:
                ori_read_acl = ori_read_acl + "," + "%s:%s" % (account, user)

            if "%s:%s" % (account, user) not in ori_write_acl:
                ori_write_acl = ori_write_acl + "," + "%s:%s" % (account, user)

            msg["Read"] = ori_read_acl
            msg["Write"] = ori_write_acl


        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, metadata_content=msg)

        if val == False:
            msg = "Failed to set metadata of the container %s: %s" % (container, msg)
            logger.error(msg)

        return Bool(val, msg)

    def remove_read_acl(self, account, container, user, admin_user, retry=3):
        '''
        Remove the user from the read acl of the container.

        @type  account: string
        @param account: the account of the user and admin_user
        @type  container: string
        @param container: the container to remove the read acl
        @type  user: string
        @param user: the user to remove from the read acl
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the user is successfully removed from the read ACL, then val == True
            and msg == "". Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="remove_read_acl")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        ori_read_acl = ""
        ori_write_acl = ""
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            return Bool(val, msg)
        else:
            if container not in msg:
                val = False
                msg = "Container %s does not exist!" % container
                return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to get metadata of the container %s: %s" % (container, msg)
            logger.error(msg)
            return Bool(val, msg)
        else:
            ori_read_acl = msg["Read"].split(",")
            ori_write_acl = msg["Write"].split(",")
            new_read_acl = ""
            new_write_acl = ""
            account_user_pattern = account + ":" + user

        while account_user_pattern in ori_read_acl:
            ori_read_acl.remove(account_user_pattern)

        while account_user_pattern in ori_write_acl:
            ori_write_acl.remove(account_user_pattern)

        for item in ori_read_acl:
            new_read_acl = new_read_acl + item + ","

        for item in ori_write_acl:
            new_write_acl = new_write_acl + item + ","

        msg["Read"] = new_read_acl
        msg["Write"] = new_write_acl

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, metadata_content=msg)

        if val == False:
            msg = "Failed to set metadata of the container %s: %s" % (container, msg)
            logger.error(msg)

        return Bool(val, msg)

    def remove_write_acl(self, account, container, user, admin_user, retry=3):
        '''
        Remove the user from the write acl of the container.

        @type  account: string
        @param account: the account of the user and admin_user
        @type  container: string
        @param container: the container to remove the write acl
        @type  user: string
        @param user: the user to remove from the write acl
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the user is successfully removed from the write ACL, then val == True
            and msg == "". Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="remove_write_acl")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        ori_read_acl = ""
        ori_write_acl = ""
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            return Bool(val, msg)
        else:
            if container not in msg:
                val = False
                msg = "Container %s does not exist!" % container
                return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to get metadata of the container %s: %s" % (container, msg)
            logger.error(msg)
            return Bool(val, msg)
        else:
            ori_write_acl = msg["Write"].split(",")
            new_write_acl = ""
            account_user_pattern = account + ":" + user

        while account_user_pattern in ori_write_acl:
            ori_write_acl.remove(account_user_pattern)

        for item in ori_write_acl:
            new_write_acl = new_write_acl + item + ","

        msg["Write"] = new_write_acl

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, metadata_content=msg)

        if val == False:
            msg = "Failed to set metadata of the container %s: %s" % (container, msg)
            logger.error(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __create_container(self, proxyIp, account, container, admin_user, admin_password):
        logger = util.getLogger(name="__create_container")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = "Fail to create the container %s: " % container
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s post %s" % (url, account, admin_user, admin_password, container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def create_container(self, account, container, admin_user, retry=3):
        '''
        Create a new container in the account by admin_user.

        @type  account: string
        @param account: the account to create a container
        @type  container: string
        @param container: the container to be created
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the creation is successfully done, then val == True and msg == "".
            Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="create_container")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            return Bool(val, msg)
        else:
            if container in msg:
                val = False
                msg = "Container %s has existed!" % container
                return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__create_container, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to create the container %s: %s" % (container, msg)
            logger.error(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __delete_container(self, proxyIp, account, container, admin_user, admin_password):
        logger = util.getLogger(name="__delete_container")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = "Fail to delete the container %s: " % container
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s delete %s" % (url, account, admin_user, admin_password, container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def delete_container(self, account, container, admin_user, retry=3):
        '''
        Delete a container in the account by admin_user.

        @type  account: string
        @param account: the account to delete a container
        @type  container: string
        @param container: the container to be deleted
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the deletion is successfully done, then val == True
            and msg == "". Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="delete_container")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            return Bool(val, msg)
        else:
            if container not in msg:
                val = False
                msg = "Container %s does not exist!" % container
                return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_container, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to delete the container %s: %s" % (container, msg)
            logger.error(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __set_container_metadata(self, proxyIp, account, container, admin_user, admin_password, metadata_content):
        '''
        Set the metadata of the given container.
        The metadata are associatied with a user and include::
            (1) Account-Enable: True/False
            (2) User-Enable: True/False
            (3) Password: the original password for the user
            (4) Quota: quota of the user (Number of bytes, int)
            (5) Description: the description about the owner of the container
            (6) Read: read ACL list
            (7) Write: write ACL list
            (8) Usage: the usage of the quota

        The following is the details of metadata_content::
            metadata_content = {
                "Read": read ACL list,
                "Write": write ACL list,
                "Account-Enable": True/False,
                "User-Enable": True/False,
                "Password": user password,
                "Quota": number of bytes,
                "Description": string,
                "Usage": number of bytes,
            }

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the account of the container
        @type  container: string
        @param container: the container to set metadata
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  admin_password: string
        @param admin_password: the password of admin_user
        @type  metadata_content: dictionary
        @param metadata_content: the content to be set to metadata of the container
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the metadata are successfully set, then val == True and msg == "".
            Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__set_container_metadata")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = "Failed to set the metadata of the container %s: " % container
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s post %s" % (url, account, admin_user, admin_password, container)

        #TODO: check whether the format of metadata_content is correct
        for field, value in metadata_content.items():
            if field == "Read":
                cmd = cmd + " -r \'%s\'" % value
            elif field == "Write":
                cmd = cmd + " -w \'%s\'" % value
            else:
                cmd = cmd + " -m \'%s:%s\'" % (field, value)

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __get_container_metadata(self, proxyIp, account, container, admin_user, admin_password):
        '''
        Get the metadata of the given container as a dictionary.
        The metadata are associatied with a user and include::
            (1) Account-Enable: True/False
            (2) User-Enable: True/False
            (3) Password: the original password for the user
            (4) Quota: quota of the user (Number of bytes, int)
            (5) Description: the description about the owner of the container
            (6) Read ACL: read ACL list
            (7) Write ACL: write ACL list
            (8) Usage: the usage of the quota

        The following is the details of metadata::
            {
                "Read": read ACL list,
                "Write": write ACL list,
                "Account-Enable": True/False,
                "User-Enable": True/False,
                "Password": user password,
                "Quota": number of bytes,
                "Description": string,
                "Usage": number of bytes,
            }

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the account of the container
        @type  container: string
        @param container: the container to get metadata
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  admin_password: string
        @param admin_password: the password of admin_user
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the metadata are successfully got, then val == True and msg
            records the metadata. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_container_metadata")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = "Failed to get the metadata of the container %s: " % container
        val = False
        metadata_content = {}
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s stat %s" % (url, account, admin_user, admin_password, container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
            return Bool(val, msg)
        else:
            lines = stdoutData.split("\n")

        for line in lines:
            if "Meta" in line:
                if line.split()[2] == "True":
                    metadata_content[line.split()[1][:-1]] = True
                elif line.split()[2] == "False":
                    metadata_content[line.split()[1][:-1]] = False
                else:
                    metadata_content[line.split()[1][:-1]] = line.split()[2]
            elif "Read" in line:
                metadata_content["Read"] = line.split("ACL: ")[1]
            elif "Write" in line:
                metadata_content["Write"] = line.split("ACL: ")[1]

        val == True
        msg = metadata_content
        logger.info(msg)

        return Bool(val, msg)


if __name__ == '__main__':
    SA = SwiftAccountMgr()
    #print SA.add_account("account1")
    #print SA.add_account("account3")
    #print SA.add_user("account1","user1")
    #print SA.add_user("account3","user1")
    print SA.delete_account("account1")
    #print SA.add_account("account4")
    #print SA.delete_user("account3","user1")
    #print SA.delete_user("account2","user1")
    #print SA.add_user("account1","user1")
    #print SA.add_user("account8","user1")
