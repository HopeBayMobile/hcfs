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

        self.__admin_name_suffix = "_admin"
        self.__admin_default_password = "admin"
        self.__user_default_password = "user"
        self.__metadata_container_suffix = "_metadata_container"
        self.__private_container_suffix = "_private_container"
        self.__shared_container_suffix = "_shared_container"

        if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
            os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

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
        @return: a tuple (val, msg). If fn is executed successfully,
            then val == True and msg records the standard output. Otherwise,
            val == False and msg records the error message.
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
                #logger.error(errMsg)
                msg = msg + '\n' + errMsg

        return (val, msg)

    @util.timeout(300)
    def __add_user(self, proxyIp, account, user, password, admin=False, reseller=False):
        logger = util.getLogger(name="__add_user")

        url = "https://%s:8080/auth/" % proxyIp
        msg = ""
        val = False

        admin_opt = "-a " if admin else ""
        reseller_opt = "-r " if reseller  else ""
        optStr = admin_opt + reseller_opt

        cmd = "swauth-add-user -K %s -A %s %s %s %s %s" % (self.__password, url, optStr, account, user, password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

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

    def add_user(self, account, user, password="", description="", admin=False, reseller=False, retry=3):
        '''
        Add a user into a given account, including the following steps::
            (1) Add a user
            (2) Create a private container for the user by the admin user
            (3) Create a metadata container for the user stored in the admin user's container
            (4) Set quota, ACL, and metdata in the metadata container for the user

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
        @return: a tuple Bool(val, msg). If the user is successfully added to
            both the database and backend swift then Bool.val == True and msg
            records the standard output. Otherwise, val == False and msg
            records the error message.
        '''

        logger = util.getLogger(name="add_user")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        admin_user = account + self.__admin_name_suffix

        #TODO: need to check the characters of the password
        if password == "":
            password = self.__user_default_password

        admin_password = self.get_user_password(account, admin_user).msg
        container = container + self.__private_container_suffix

        metadata_content = {
                "Account-Enable": True,
                "User-Enable": True,
                "Password": password,
                "Quota": 0,
                "Description": description,
        }

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

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__add_user, account=account,\
                                           user=user, password=password, admin=admin, reseller=reseller)

        if val == False:
            msg = "Failed to add user: " + msg
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=account, admin_user=admin_user, admin_password=admin_password,\
                                           container=container, metadata_content=metadata_content)

        if val == False:
            #TODO: need to rollback
            msg = "Failed to set container metadata: " + msg
            logger.error(msg)
            return Bool(val, msg)

        assign_write_acl_output = self.assign_write_acl(account=account, container=container, user=user, admin_user=admin_user)

        if assign_write_acl_output.val == False:
            #TODO: need to rollback
            val == False
            msg = "Failed to assign write acl: " + msg
            logger.error(msg)
        else:
            val = True
            msg = ""

        return Bool(val, msg)

    @util.timeout(300)
    def __delete_user(self, proxyIp, account, user):
        logger = util.getLogger(name="__delete_user")

        url = "https://%s:8080/auth/" % proxyIp

        cmd = "swauth-delete-user -K %s -A %s %s %s" % (self.__password, url, account, user)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        msg = ""
        val = False

        if po.returncode != 0 and '404' not in stderrData:
            logger.error(stderrData)
            msg = stderrData
            val = False
        elif '404' in stderrData:
            msg = "User %s:%s does not exist" % (account, user)
            logger.warn(msg)
            val = True
        else:
            logger.info(stdoutData)
            msg = stdoutData
            val = True

        Bool = collections.namedtuple("Bool", "val msg")
        return Bool(val, msg)

    def delete_user(self, account, user, retry=3):
        '''
        Delete user from the database and backend swift

        @type  account: string
        @param account: the name of the given account
        @type  user: string
        @param user: the name of the given user
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user is successfully
            deleted to both the database and backend swift then
            Bool.val == True and msg records the standard output. Otherwise,
            val == False and msg records the error message.
        '''

        logger = util.getLogger(name="delete_user")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_user, account=account, user=user)

        if val == False:
            msg = "Failed to delete user: " + msg
            logger.error(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __add_account(self, proxyIp, account):
        logger = util.getLogger(name="__add_account")

        url = "https://%s:8080/auth/" % proxyIp
        msg = ""
        val = False

        cmd = "swauth-add-account -K %s -A %s %s" % (self.__password, url, account)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

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

    def add_account(self, account, quota, admin_user="", admin_password="", description="", retry=3):
        '''
        Add a new account, including the following things::
            (1) Create the account and the admin user
            (2) Create the private container for the admin user
            (3) Create the metadata container for the admin user stored in super_admin's container
            (4) Update the metadata and set the quota for the account

        @type  account: string
        @param account: the name of the given account
        @type  quota: string
        @param quota: the quota of the account
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
            admin_user = account + self.__admin_name_suffix

        #TODO: need to check the characters of the password
        if admin_password == "":
            admin_password = self.__admin_default_password

        admin_metadata = {
            "Account-Enable": True,
            "User-Enable": True,
            "Quota": quota,
            "Password": admin_password,
            "Description": description,
        }

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

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__add_user, account=account,\
                                           user=admin_user, password=admin_password, admin=True, reseller=False)

        if val == False:
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=".super_admin", container=account, admin_user=".super_admin",\
                                           admin_password=self.__password, metadata_content=admin_metadata)

        if val == False:
            #TODO: need to roll-back
            logger.error(msg)
            return Bool(val, msg)
        else:
            return Bool(val, msg)

    @util.timeout(300)
    def __delete_account(self, proxyIp, account):
        logger = util.getLogger(name="__delete_account")
        url = "https://%s:8080/auth/" % proxyIp

        cmd = "swauth-delete-account -K %s -A %s %s" % (self.__password, url, account)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        msg = ""
        val = False

        if po.returncode != 0 and '409' not in stderrData and '404' not in stderrData:
            logger.error(stderrData)
            msg = stderrData
            val = False
        elif '404' in stderrData:
            msg = "Account %s does not exist" % account
            logger.warn(msg)
            val = False
        elif '409' in stderrData:
            msg = "Still have user(s) in account %s." % (account)
            logger.warn(msg)
            val = False
        else:
            logger.info(stdoutData)
            msg = stdoutData
            val = True

        Bool = collections.namedtuple("Bool", "val msg")
        return Bool(val, msg)

    def delete_account(self, account, retry=3):
        '''
        Delete an account from backend swift after checking that there's no users in the account.

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
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        list_user_output = self.list_user(account)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_account, account=account)

        if val == False:
            msg = "Failed to delete account: " + msg
            logger.error(msg)

        return Bool(val, msg)

    def enable_user(self, account, container, user, admin_user, retry=3):
        '''
        Enable the user to access the backend Swift by restoring the original
        password kept in the metadata of the user's container.

        @type  account: string
        @param account: the account of the user
        @type  container: string
        @param container: the container for the user
        @type  user: string
        @param user: the user to be enabled
        @type  admin_user: string
        @param admin_user: the admin user of the container
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the user's password is
            successfully restored to the original password kept in the
            metadata of the user's container, then Bool.val = True and
            Bool.msg = the standard output. Otherwise, Bool.val == False
            and Bool.msg indicates the error message.
        '''
        logger = util.getLogger(name="enable_user")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        ori_user_password = ""
        actual_user_password = ""
        admin_password = ""
        container_metadata = {}

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

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        #TODO: check whehter the container is associated with the user
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_container_metadata,\
                                           account=account,\
                                           container=container,\
                                           admin_user=admin_user,\
                                           admin_password=admin_password)

        if val == False:

            msg = "Failed to get the metadata of the container %s"\
                  % container + msg

            return Bool(val, msg)

        else:
            container_metadata = msg

        if container_metadata["Account-Enable"] == False:
            val = False

            msg = "Failed to enable user %s: account %s does not enable"\
            % (user, account)

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

        change_password_output = self.change_password(account, user,\
                                                      actual_user_password,\
                                                      ori_user_password)

        if change_password_output.val == False:
            val = False
            msg = change_password_output.msg
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__set_container_metadata,\
                                           account=account,\
                                           container=container,\
                                           admin_user=admin_user,\
                                           admin_password=admin_password,\
                                           metadata_content=container_metadata)

        return Bool(val, msg)

    def disable_user(self, account, container, user, admin_user, retry=3):
        '''
        Disable the user to access the backend Swift by changing the password
        to a random string. The original password will be stored in the
        metadata of the user's container.

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
        @return: a tuple Bool(val, msg). If the user's password is successfully
            changed and the original password is stored in the metadata of
            the user's container, then Bool.val = True and Bool.msg = the
            standard output. Otherwise, Bool.val == False and Bool.msg
            indicates the error message.
        '''
        logger = util.getLogger(name="disable_user")
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        new_user_password = str(uuid.uuid4())
        actual_user_password = ""
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

        get_user_password_output = self.get_user_password(account, user)
        if get_user_password_output.val == False:
            val = False
            msg = get_user_password_output.msg
            return Bool(val, msg)
        else:
            actual_user_password = get_user_password_output.msg

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        #TODO: check whehter the container is associated with the user

        change_password_output = self.change_password(account, user,\
                                                      actual_user_password,\
                                                      new_user_password)

        if change_password_output.val == False:
            val = False
            msg = change_password_output.msg
            return Bool(val, msg)

        container_metadata = {"User-Enable": False,
                              "Password": actual_user_password}

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__set_container_metadata,\
                                           account=account,\
                                           container=container,\
                                           admin_user=admin_user,\
                                           admin_password=admin_password,\
                                           metadata_content=container_metadata)

        return Bool(val, msg)

    def enable_account(self, account, retry=3):
        '''
        Enable the account by changing the passwords of all users from random
        password to original password saved in the metadata. Note that after
        changing all users' passwords, the metadata must be updated.

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
        passwords, the metadata must be updated.

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
    def __change_password(self, proxyIp, account, user,\
                          newPassword, admin=True, reseller=False):
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
        @return: a tuple Bool(val, msg). If the operation is successfully
            done, then val == True and msg records the standard output.
            Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__change_password")

        url = "https://%s:8080/auth/" % proxyIp
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        admin_opt = "-a " if admin else ""
        reseller_opt = "-r " if reseller  else ""
        optStr = admin_opt + reseller_opt

        #TODO: Must fix the format of password to use special character
        cmd = "swauth-add-user -K %s -A %s %s %s %s %s"\
              % (self.__password, url, optStr, account, user, newPassword)

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,\
                              stderr=subprocess.PIPE)

        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            msg = "Failed to change the password of %s: %s"\
                  % (user, stderrData)

            logger.error(msg)
            val = False

        else:
            msg = stdoutData
            logger.info(msg)
            val = True

        return Bool(val, msg)

    def change_password(self, account, user,\
                        oldPassword, newPassword, retry=3):
        '''
        Change the password of a Swift user in a given account.
        The original password of the user must be provided for identification.

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
        @return: a named tuple Bool(val, msg). If the password is changed
            successfully, then Bool.val == True and msg == "". Otherwise,
            Bool.val == False and Bool.msg records the error message.
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
        if user_existence_output.val == True:
            if user_existence_output.result == False:
                val = False
                msg = "User %s does not exist!" % user
                return Bool(val, msg)
        else:
            val = False
            msg = user_existence_output.msg
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_user_detail,\
                                           account=account, user=user)

        if val == False:
            return Bool(val, msg)

        try:
            user_detail = json.loads(msg)
            msg = ""

        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            val = False
            return Bool(val, msg)

        for item in user_detail["groups"]:
            if item["name"] == ".admin":
                admin = True
            if item["name"] == ".reseller_admin":
                reseller = True

        ori_password = user_detail["auth"].split(":")[1]

        if oldPassword != ori_password:
            val = False
            msg = "Authentication failed! The old password is not correct!"
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__change_password,\
                                           account=account, user=user,\
                                           newPassword=newPassword,\
                                           admin=admin, reseller=reseller)

        if val == False:
            return Bool(val, msg)

        admin_user = ""  # to be defined
        admin_password = ""
        user_container = ""  # to be defined
        container_metadata = {"Password": newPassword}

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False or\
           get_admin_password_output.msg == "":

            val = False

            msg = "Failed to get the password of the admin user: %s"\
                  % admin_user

            return Bool(val, msg)

        else:
            admin_password = get_admin_password_output.msg

        (val, msg) = self.__functionBroker(\
                        proxy_ip_list=proxy_ip_list, retry=retry,\
                        fn=self.__set_container_metadata, account=account,\
                        container=user_container, admin_user=admin_user,\
                        admin_password=admin_password,\
                        metadata_content=container_metadata)

        return Bool(val, msg)

    @util.timeout(300)
    def __get_account_usage(self, proxyIp, account, user):
        '''
        Return the statement of the given user in the give account.

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the name of the given account
        @type  user: string
        @param user: the name of the given user
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operatoin is successfully
            done, then val == True and msg records the information
            of the given user. Otherwise, val == False and msg
            records the error message.
        '''
        logger = util.getLogger(name="__get_account_user")

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
        get account usage from the backend swift

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
        @return: a tuple Bool(val, msg). If the operatoin is successfully
            done, then val == True and msg records the information
            of the given user. Otherwise, val == False and msg
            records the error message.
        '''
        logger = util.getLogger(name="__get_user_detail")

        url = "https://%s:8080/auth/" % proxyIp
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swauth-list -K %s -A %s %s %s"\
              % (self.__password, url, account, user)

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,\
                              stderr=subprocess.PIPE)

        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            msg = stderrData
            logger.error(msg)
            val = False

        else:
            msg = stdoutData
            logger.info(msg)
            val = True

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
        @return: a named tuple Bool(val, msg). If get the user's password
            successfully, then Bool.val == True, and Bool.msg == password.
            Otherwise, Bool.val == False, and Bool.msg records the
            error message.
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

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_user_detail,\
                                           account=account, user=user)

        if val == False:
            return Bool(val, msg)

        try:
            user_detail = json.loads(msg)
            val = True
            msg = ""

        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            val = False
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
        Return whether the given user is admin.

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
        Return whether the given user is reseller admin.

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
        in the container for the admin user of the given account.

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
        the container for the admin user of the given account.

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
        the container for the user.

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

        (val, msg) = self.__functionBroker(\
                        proxy_ip_list=proxy_ip_list, retry=retry,\
                        fn=self.__set_container_metadata, account=account,\
                        container=container, admin_user=admin_user,\
                        admin_password=admin_password,\
                        metadata_content=container_metadata)

        return Bool(val, msg)

    def get_user_quota(self, account, container, user, admin_user, retry=3):
        '''
        Get the quota of the given user by reading the metadata in
        the container for the user.

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

        (val, msg) = self.__functionBroker(\
                        proxy_ip_list=proxy_ip_list, retry=retry,\
                        fn=self.__get_container_metadata, account=account,\
                        container=container, admin_user=admin_user,\
                        admin_password=admin_password)

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
        @return: a tuple Bool(val, msg). If the operation is successfully
            done, then val == True and msg will record the
            information. Otherwise, val == False, and msg will
            record the error message.
        '''
        logger = util.getLogger(name="__get_account_info")

        url = "https://%s:8080/auth/" % proxyIp
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swauth-list -K %s -A %s" % (self.__password, url)

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,\
                              stderr=subprocess.PIPE)

        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            msg = stderrData
            logger.error(msg)
            val = False

        else:
            msg = stdoutData
            val = True

        return Bool(val, msg)

    def account_existence(self, account, retry=3):
        '''
        Check whether the given account exists.

        @type  account: string
        @param account: an account name to be queried
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(result, val, msg). If the account
            exists, then Bool.result == True, Bool.val == True,
            and Bool.msg == "". If the account does not exist, then
            Bool.result == False, Bool.val == True, and Bool.msg == "".
            Otherwise, Bool.result == False, Bool.val == False, and
            Bool.msg records the error message.
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

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_account_info)

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
            if item["name"] == account:
                result = True

        return Bool(result, val, msg)

    def list_account(self, retry=3):
        '''
        List all the existed accounts.

        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(result, val, msg). If the get the account
            list successfully, then Bool.val == True, and
            Bool.msg == account list. If the account list does not exist,
            then Bool.val == True, and Bool.msg == "". Otherwise,
            Bool.val == False, and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="list_account")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        account_info = {}
        account_list = ""
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_account_info)

        if val == False:
            return Bool(val, msg)

        try:
            account_info = json.loads(msg)
            val = True
            msg = ""

        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            result = False
            val = False
            return Bool(val, msg)

        for item in account_info["accounts"]:
            account_list += item["name"] + ", "
            msg = account_list

        print "DB: "
        print self.__accountDb.show_user_info_table()

        return Bool(val, msg)

    def list_user(self, account, retry=3):
        '''
        List all the existed accounts.

        @type  account: string
        @param account: the account name of the given user
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the get the user
            list successfully, then Bool.val == True, and
            Bool.msg == user list. If the user list does not exist, then
            Bool.val == True, and Bool.msg == "". Otherwise,
            Bool.val == False, and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="list_user")

        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        user_info = {}
        user_list = ""
        val = False
        msg = ""

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

        if not msg:
            msg = "account %s does not exist" % account
            return Bool(val, msg)

        else:
            try:
                user_info = json.loads(msg)
                val = True
                msg = ""

            except Exception as e:
                msg = "Failed to load the json string: %s" % str(e)
                logger.error(msg)
                val = False
                return Bool(val, msg)

            if not user_info["users"]:
                msg = "No user exists in account %s" % account
                val = False
                return Bool(val, msg)

            else:
                for item in user_info["users"]:
                    user_list += item["name"] + ", "
                    msg = user_list

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
        @return: a named tuple Bool(val, msg). If the container list is got
            successfully, then Bool.val == True and Bool.msg == user list.
            If the container list does not exist, then Bool.val == True
            and Bool.msg == "". Otherwise, Bool.val == False, and
            Bool.msg records the error message.
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

        get_user_password_output = self.get_user_password(account, admin_user)
        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False

            msg = "Failed to get the password of the admin user %s: %s"\
                  % (admin_user, get_user_password_output.msg)

            return Bool(val, msg)

        (val, msg) = self.__functionBroker(\
                        proxy_ip_list=proxy_ip_list, retry=retry,\
                        fn=self.__get_container_info, account=account,\
                        admin_user=admin_user, admin_password=admin_password)

        if val == False or msg == "":
            return Bool(val, msg)

        msg = msg.split("\n")
        msg.remove("")

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
        @return: a tuple Bool(val, msg). If the operation is successfully
            done, then val == True and msg records the container
            information. Otherwise, val == False and msg records
            the error message.
        '''
        logger = util.getLogger(name="__get_container_info")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s list" % (url, account, admin_user, admin_password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            msg = stderrData
            logger.error(msg)
            val = False
        else:
            msg = stdoutData
            val = True

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
        @return: a tuple Bool(val, msg). If the operation is successfully
            done, then val == True and msg records the user
            information. Otherwise, val == False and msg records
            the error message.
        '''
        logger = util.getLogger(name="__get_user_info")

        url = "https://%s:8080/auth/" % proxyIp
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swauth-list -K %s -A %s %s" % (self.__password, url, account)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 and '404' not in stderrData:
            logger.error(stderrData)
            msg = stderrData
            val = False
        elif '404' in stderrData:
            msg = ""
            logger.warn(msg)
            val = True
        else:
            logger.info(stdoutData)
            msg = stdoutData
            val = True

        return Bool(val, msg)

    def user_existence(self, account, user, retry=3):
        '''
        Check whether the given user exists.

        @type  account: string
        @param account: the account name of the given user
        @type  user: string
        @param user: the user to be checked
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(result, val, msg). If the user exists,
            then Bool.result == True, Bool.val == True, and Bool.msg == "".
            If the user does not exist, then Bool.result == False,
            Bool.val == True, and Bool.msg == "". Otherwise, Bool.result
            == False, Bool.val == False, and Bool.msg records the error
            message.
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

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_user_info, account=account)

        if val == False:
            result = False
            return Bool(result, val, msg)

        try:
            user_info = json.loads(msg)
            val = True
            msg = ""
        except Exception as e:
            msg = "Failed to load the json string: %s" % str(e)
            logger.error(msg)
            result = False
            val = False
            return Bool(result, val, msg)

        for item in user_info["users"]:
            if item["name"] == user:
                result = True

        return Bool(result, val, msg)

    @util.timeout(300)
    def __get_container_acl(self, proxyIp, account, container, admin_user, admin_password):
        '''
        Get ACL of the container

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the account of the container
        @type  container: string
        @param container: the container to get the read acl
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  admin_password: string
        @param admin_password: the password of admin_user
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the read acl is successfully
            gotten, then val == True and msg == "". Otherwise, val ==
            False and msg records the error message.
        '''

        logger = util.getLogger(name="__get_container_acl")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        acl = {}
        msg = "Failed to get ACL of container %s:" % container
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s stat %s" % (url, account, admin_user, admin_password, container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            msg = msg + " " + stderrData
            logger.error(msg)
            val = False
            return Bool(val, msg)

        lines = stdoutData.split("\n")

        for line in lines:
            if "Read" in line:
                acl["Read"] = line.split("ACL: ")[1]
            elif "Write" in line:
                acl["Write"] = line.split("ACL: ")[1]

        if acl.get("Read") is None or acl.get("Write") is None:
            msg = msg + " " + stderrData
            logger.error(msg)
            val = False
        else:
            val = True
            msg = acl

        return Bool(val, msg)

    @util.timeout(300)
    def __set_container_acl(self, proxyIp, account, container, admin_user, admin_password, read_acl, write_acl):
        '''
        Set ACL of the container

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the account of the container
        @type  container: string
        @param container: the container to set the write acl
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  admin_password: string
        @param admin_password: the password of admin_user
        @type  read_acl: string
        @param read_acl: the read acl to be set to that of the container
        @type  write_acl: string
        @param write_acl: the write acl to be set to that of the container
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the write acl is successfully
            set, then val == True and msg == "". Otherwise, val ==
            False and msg records the error message.
        '''

        logger = util.getLogger(name="__set_container_acl")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = "Failed to set ACL of container %s:" % container
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s post -r \'%s\' -w \'%s\' %s"\
              % (url, account, admin_user, admin_password, read_acl, write_acl, container)

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            msg = msg + " " + stderrData
            logger.error(msg)
            val = False
            return Bool(val, msg)
        else:
            msg = stdoutData
            logger.info(msg)
            val = True

        return Bool(val, msg)

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
        @return: a named tuple Bool(val, msg). If the read acl is
            successfully assigned, then val == True and msg == "".
            Otherwise, val == False and msg records the error message.
        '''

        logger = util.getLogger(name="assign_read_acl")

        #TODO: Check the existence of container
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

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_acl, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to get the container ACL: " + msg
            logger.error(msg)
            return Bool(val, msg)
        else:
            ori_read_acl = msg["Read"]
            ori_write_acl = msg["Write"]

            if "%s:%s" % (account, user) in ori_read_acl:
                val = True
                msg = ""
                return Bool(val, msg)

            ori_read_acl = ori_read_acl + "," + "%s:%s" % (account, user)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_acl,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, read_acl=ori_read_acl, write_acl=ori_write_acl)

        if val == False:
            msg = "Failed to set the container ACL: " + msg
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
        @return: a named tuple Bool(val, msg). If the write acl is
            successfully assigned, then val == True and msg == "".
            Otherwise, val == False and msg records the error message.
        '''

        logger = util.getLogger(name="assign_write_acl")

        #TODO: Check the existence of container
        proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
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

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_acl, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to get the container ACL: " + msg
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

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_acl,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, read_acl=ori_read_acl, write_acl=ori_write_acl)

        if val == False:
            msg = "Failed to set the container ACL: " + msg
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
        @return: a named tuple Bool(val, msg). If the read acl is successfully
            removed, then val == True and msg == "". Otherwise, val ==
            False and msg records the error message.
        '''

        logger = util.getLogger(name="remove_read_acl")

        #TODO: Check the existence of container
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

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_acl, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to get the container ACL: " + msg
            logger.error(msg)
            return Bool(val, msg)
        else:
            ori_read_acl = msg["Read"]
            ori_write_acl = msg["Read"]

        new_read_acl = ""
        new_write_acl = ""
        ori_read_acl = ori_read_acl.split(",")
        ori_write_acl = ori_write_acl.split(",")
        account_user_pattern = account + ":" + user

        while account_user_pattern in ori_read_acl:
            ori_read_acl.remove(account_user_pattern)

        while account_user_pattern in ori_write_acl:
            ori_write_acl.remove(account_user_pattern)

        for item in ori_read_acl:
            new_read_acl = new_read_acl + item + ","

        for item in ori_write_acl:
            new_write_acl = new_write_acl + item + ","

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_acl,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, read_acl=new_read_acl, write_acl=new_write_acl)

        if val == False:
            msg = "Failed to set the container ACL: " + msg
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
        @return: a named tuple Bool(val, msg). If the write acl is successfully
            removed, then val == True and msg == "". Otherwise, val ==
            False and msg records the error message.
        '''

        logger = util.getLogger(name="remove_write_acl")

        #TODO: Check the existence of container
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

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = "Failed to get the password of the admin user %s: %s" % (admin_user, get_user_password_output.msg)
            logger.error(msg)
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_acl, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            msg = "Failed to get the container ACL: " + msg
            logger.error(msg)
            return Bool(val, msg)

        ori_read_acl = msg["Read"]
        ori_write_acl = msg["Write"]

        new_write_acl = ""
        ori_write_acl = ori_write_acl.split(",")
        account_user_pattern = account + ":" + user

        while account_user_pattern in ori_write_acl:
            ori_write_acl.remove(account_user_pattern)

        for item in ori_write_acl:
            new_write_acl = new_write_acl + item + ","

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_acl,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, read_acl=ori_read_acl, write_acl=new_write_acl)

        if val == False:
            msg = "Failed to set container ACL: " + msg
            logger.error(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __create_container(self, proxyIp, account, container, admin_user, admin_password):
        logger = util.getLogger(name="__create_container")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = "Fail to create container %s" % container
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s post %s" % (url, account, admin_user, admin_password, container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            msg = stderrData
            logger.error(msg)
            val = False
        else:
            msg = stdoutData
            val = True

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
        @return: a named tuple Bool(val, msg). If the creation is successfully
            done, then val == True and msg == "". Otherwise, val ==
            False and msg records the error message.
        '''

        logger = util.getLogger(name="create_container")

        #TODO: Check whether the container exists
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
            msg = "Failed to create the container: " + msg
            logger.error(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __set_container_metadata(self, proxyIp, account, container, admin_user, admin_password, metadata_content):
        '''
        Set self-defined metadata of the given container.
        The self-defined metadata are associatied with a user and include::
            (1) Account-Enable: True/False
            (2) User-Enable: True/False
            (3) Password: the original password for the user
            (4) Quota: quota of the user (Number of bytes, int)
            (5) Description: the description about the owner of the container

        The following is the details of metadata_content::
            metadata_content = {
                "Account-Enable": True/False,
                "User-Enable": True/False,
                "Password": user password,
                "Quota": number of bytes,
                "Description": string,
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
        @param metadata_content: the content to be set to metadata
            of the container
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the metadata are successfully
            set, then val == True and msg == "". Otherwise, val ==
            False and msg records the error message.
        '''

        logger = util.getLogger(name="__set_container_metadata")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = "Failed to set the metadata of container %s:" % container
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s post %s" % (url, account, admin_user, admin_password, container)

        #TODO: check whether the format of metadata_content is correct
        for field, value in metadata_content.items():
            cmd = cmd + " -m \'%s:%s\'" % (field, value)

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            msg = msg + " " + stderrData
            logger.error(msg)
            val = False
            return Bool(val, msg)
        else:
            msg = stdoutData
            logger.info(msg)
            val = True

        return Bool(val, msg)

    @util.timeout(300)
    def __get_container_metadata(self, proxyIp, account, container, admin_user, admin_password):
        '''
        Get self-defined metadata of the given container as a dictionary.
        The self-defined metadata are associatied with a user and include::
            (1) Account-Enable: True/False
            (2) User-Enable: True/False
            (3) Password: the original password for the user
            (4) Quota: quota of the user (Number of bytes, int)
            (5) Description: the description about the owner of the container

        The following is the details of metadata::
            {
                "Account-Enable": True/False,
                "User-Enable": True/False,
                "Password": user password,
                "Quota": number of bytes,
                "Description": string
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
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the metadata are successfully
            got, then val == True and msg records the metadata. Otherwise,
            val == False and msg records the error message.
        '''

        logger = util.getLogger(name="__get_container_metadata")

        url = "https://%s:8080/auth/v1.0" % proxyIp
        msg = "Failed to get the metadata of container %s:" % container
        val = False
        metadata_content = {}
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s stat %s" % (url, account, admin_user, admin_password, container)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0:
            msg = msg + " " + stderrData
            logger.error(msg)
            val = False
            return Bool(val, msg)

        lines = stdoutData.split("\n")

        for line in lines:
            if "Meta" in line:
                val = True

                if line.split()[2] == "True":
                    metadata_content[line.split()[1][:-1]] = True
                elif line.split()[2] == "False":
                    metadata_content[line.split()[1][:-1]] = False
                else:
                    metadata_content[line.split()[1][:-1]] = line.split()[2]

        msg = metadata_content
        logger.info(msg)

        if val == False:
            msg = stderrData

        return Bool(val, msg)


if __name__ == '__main__':
    SA = SwiftAccountMgr()

    print SA.list_account().msg
    print SA.add_account("ricetest09").msg
    print SA.list_account().msg
    print SA.list_user("ricetest09").msg
