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
import fcntl

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


class InconsistentDatabaseError(Exception):
    pass


class Lock:
    def __init__(self, filename):
        self.filename = filename
        # This will create it if it does not exist already
        self.handle = open(filename, 'w')

    # Bitwise OR fcntl.LOCK_NB if you need a non-blocking lock 
    def acquire(self):
        fcntl.flock(self.handle, fcntl.LOCK_EX)

    def release(self):
        fcntl.flock(self.handle, fcntl.LOCK_UN)

    def __del__(self):
        self.handle.close()

lock = Lock("/tmp/swift_account_lock.tmp")

class SwiftAccountMgr:
    def __init__(self, conf=GlobalVar.ORI_SWIFTCONF):
        logger = util.getLogger(name="SwiftAccountMgr.__init__")

        if os.path.isfile(conf):
            cmd = "cp %s %s" % (conf, GlobalVar.SWIFTCONF)
            os.system(cmd)
        else:
            msg = "Confing %s does not exist!" % conf
            print >> sys.stderr, msg
            logger.warn(msg)

        if not os.path.isfile(GlobalVar.SWIFTCONF):
            msg = "Config %s does not exist!" % GlobalVar.SWIFTCONF
            print >> sys.stderr, msg
            logger.error(msg)
            sys.exit(1)

        self.__deltaDir = GlobalVar.DELTADIR
        self.__swiftDir = self.__deltaDir + "/swift"

        self.__SC = SwiftCfg(GlobalVar.SWIFTCONF)
        self.__kwparams = self.__SC.getKwparams()
        self.__password = self.__kwparams['password']
        self.__proxy_ip_list = ["127.0.0.1"]
        self.__auth_port = "8080"

        self.__admin_default_name = "admin"
        self.__random_password_size = 12
        self.__config_container_suffix = "_gateway_config"
        self.__private_container_suffix = "_private_container"
        self.__shared_container_suffix = "_shared_container"
        self.__metadata_name = ".metadata"

        if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
            os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

    def __generate_random_password(self):
        '''
        Generate a random password of length 12. The characters of the random password are alphanumeric.

        @rtype:  string
        @return: a random password
        '''
        logger = util.getLogger(name="__generate_random_password")

        chars = string.letters + string.digits
        return "".join(random.choice(chars) for x in range(self.__random_password_size))

    def __functionBroker(self, proxy_ip_list, retry, fn, **kwargs):
        '''
        Repeat at most retry times::
            (1) Execute the private function fn with a randomly chosen proxy node and kwargs as input.
            (2) Break if fn retrun True

        @type  proxy_ip_list: string
        @param proxy_ip_list: ip list of proxy nodes
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn returns False
        @type  fn: string
        @param fn: private function to call
        @param kwargs: keyword arguments to fn
        @rtype:  tuple
        @return: a tuple (val, msg). If fn is executed successfully, then val == True and msg records
            the standard output. Otherwise, val == False and msg records the error message.
        '''
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
                    errMsg = "Failed to run %s through %s: %s\n" % (fn.__name__, ip, output.msg)
                    msg = msg + errMsg

            except util.TimeoutError:
                errMsg = "Failed to run %s through %s in time.\n" % (fn.__name__, ip)
                msg = msg + errMsg

        return (val, msg)

    @util.timeout(300)
    def __add_user(self, proxyIp, account, user, password, admin=False, reseller=False):
        logger = util.getLogger(name="__add_user")

        url = "https://%s:%s/auth/" % (proxyIp, self.__auth_port)
        msg = "Failed to add user %s:%s: " % (account, user)
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

    def add_user(self, account, user, password="", description="no description", quota=0, admin=False, reseller=False, retry=3):
        '''
        Add a user into an account, including the following steps::
            (1) Add a user.
            (2) Create the user's private container.
            (3) Create the user's metadata stored in super_admin account.
            (4) Set ACL for the private container.

        @type  account: string
        @param account: the name of the account
        @type  user: string
        @param user: the name of the user
        @type  password: string
        @param password: the password to be set
        @type  description: string
        @param description: the description of the user
        @type  quota: integer
        @param quota: the quota of the user
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
        logger = util.getLogger(name="add_user")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        private_container = user + self.__private_container_suffix
        config_container = user + self.__config_container_suffix
        metadata_content = {}
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if password == "":
            password = self.__generate_random_password()

        if len(description.split()) == 0:
            msg = "Description can not be an empty string."
            return Bool(val, msg)

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        check_account_existence = self.account_existence(account)

        if check_account_existence.val == False:
            val = False
            msg = check_account_existence.msg
            lock.release()
            return Bool(val, msg)
        elif user == self.__admin_default_name:
            if check_account_existence.result == True:
                val = False
                msg = "Account %s has existed!" % account
                lock.release()
                return Bool(val, msg)
        else:
            if check_account_existence.val == False:
                val = False
                msg = check_account_existence.msg
                lock.release()
                return Bool(val, msg)
            elif check_account_existence.result == False:
                val = False
                msg = "Account %s does not exist!" % account
                lock.release()
                return Bool(val, msg)

            user_existence_output = self.user_existence(account, user)

            if user_existence_output.val == False:
                val = False
                msg = user_existence_output.msg
                lock.release()
                return Bool(val, msg)
            elif user_existence_output.result == True:
                val = False
                msg = "User %s:%s has existed!" % (account, user)
                lock.release()
                return Bool(val, msg)

            get_admin_password_output = self.get_user_password(account, self.__admin_default_name)

            if get_admin_password_output.val == False:
                val = False
                msg = get_admin_password_output.msg
                lock.release()
                return Bool(val, msg)
            else:
                admin_password = get_admin_password_output.msg

            write_acl = {
                "Read": account + ":" + user,
                "Write": account + ":" + user,
            }

        if user == self.__admin_default_name:
            metadata_content[user] = {"description": description, "quota": quota,}
        else:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=account, object_name=self.__metadata_name)

            if val == False:
                msg = "Failed to get the original metadata: " + msg
                logger.error(msg)
                lock.release()
                return Bool(val, msg)
            else:
                metadata_content = msg
                metadata_content[user] = {"description": description, "quota": quota,}

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name, object_content=metadata_content)

        if val == False:
            msg = "Failed to set the metadata: " + msg
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        elif user == self.__admin_default_name:
            val = True
        else:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                               account=account, admin_user=self.__admin_default_name, admin_password=admin_password,\
                                               container=private_container, metadata_content=write_acl)

        if val == False:
            msg = "Failed to create the private container: " + msg
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        elif user == self.__admin_default_name:
            val = True
        else:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                               account=account, admin_user=self.__admin_default_name, admin_password=admin_password,\
                                               container=config_container, metadata_content=write_acl)

        if val == False:
            msg = "Failed to create the config container: " + msg
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__add_user, account=account,\
                                               user=user, password=password, admin=admin, reseller=reseller)

        if val == False:
            logger.error(msg)
        else:
            val = True
            msg = ""

        lock.release()
        return Bool(val, msg)

    @util.timeout(300)
    def __delete_user(self, proxyIp, account, user):
        logger = util.getLogger(name="__delete_user")

        url = "https://%s:%s/auth/" % (proxyIp, self.__auth_port)
        msg = "Failed to delete user %s:%s: " % (account, user)
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
        Delete the user from backend Swift. The user's data will be destroyed.
        The deletion includes the following steps::
            (1) Delete a user.
            (2) Remove the user's metadata from super_admin account.
            (3) Delete the user's private container.

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
        logger = util.getLogger(name="delete_user")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        metadata_content = {}
        private_container = user + self.__private_container_suffix
        config_container = user + self.__config_container_suffix
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        user_existence_output = self.user_existence(account, user)

        if user_existence_output.val == False:
            val = False
            msg = user_existence_output.msg
            lock.release()
            return Bool(val, msg)
        elif user_existence_output.result == False:
            val = False
            msg = "User %s:%s does not exist!" % (account, user)
            lock.release()
            return Bool(val, msg)

        get_admin_password_output = self.get_user_password(account, self.__admin_default_name)

        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            lock.release()
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg


        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name)

        if val == False:
            msg = "Failed to get the metadata: " + msg
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            metadata_content = msg
            if metadata_content.pop(user, -1) == -1:
                logger.error("The metadata of user %s:%s do not exist!" % (account, user))

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name, object_content=metadata_content)

        if val == False:
            msg = "Failed to delete the metadata: " + msg
            logger.error(msg)
            lock.release()
            return Bool(val, msg)

        account_admin_container = self.list_container(account, self.__admin_default_name)

        if account_admin_container.val == False:
            val = False
            msg = account_admin_container.msg
            lock.release()
            return Bool(val, msg)

        if private_container in account_admin_container.msg:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_target,\
                                               account=account, target=private_container, admin_user=self.__admin_default_name,\
                                               admin_password=admin_password)
        else:
            val = True
            msg = ""

        if val == False:
            msg = "Failed to delete the private container: " + msg
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
            
        if config_container in account_admin_container.msg:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_target,\
                                               account=account, target=config_container, admin_user=self.__admin_default_name,\
                                               admin_password=admin_password)
        else:
            val = True
            msg = ""

        if val == False:
            msg = "Failed to delete the config container: " + msg
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_user, account=account, user=user)

        if val == False:
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    @util.timeout(300)
    def __add_account(self, proxyIp, account):
        logger = util.getLogger(name="__add_account")

        url = "https://%s:%s/auth/" % (proxyIp, self.__auth_port)
        msg = "Failed to add account %s: " % account
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
        @param quota: the quota of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). When the account is successfully created, Bool.val == True.
                Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="add_account")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if admin_user == "":
            admin_user = self.__admin_default_name

        if admin_password == "":
            admin_password = self.__generate_random_password()

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        if len(description.split()) == 0:
            msg = "Description can not be an empty string."
            lock.release()
            return Bool(val, msg)

        check_account_existence = self.account_existence(account)

        if check_account_existence.val == False:
            val = False
            msg = check_account_existence.msg
            lock.release()
            return Bool(val, msg)
        elif check_account_existence.result == True:
            val = False
            msg = "Account %s has existed!" % account
            lock.release()
            return Bool(val, msg)

        add_user_output = self.add_user(account=account, user=self.__admin_default_name, password="",\
                                        description=description, quota=quota, admin=True)

        if add_user_output.val == False:
            val = False
            msg = "Failed to add account administrator: " + add_user_output.msg
            logger.error(msg)
        else:
            val = True
            msg = ""

        lock.release()
        return Bool(val, msg)

    @util.timeout(300)
    def __delete_account(self, proxyIp, account):
        logger = util.getLogger(name="__delete_account")

        url = "https://%s:%s/auth/" % (proxyIp, self.__auth_port)
        msg = "Failed to delete account %s: " % account
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
        @param account: the name of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the account is successfully deleted, then Bool.val ==
                True. Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="delete_account")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        msg = ""
        val = False
        user_list = []
        black_list = []
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        check_account_existence = self.account_existence(account)

        if check_account_existence.val == False:
            val = False
            msg = check_account_existence.msg
            lock.release()
            return Bool(val, msg)
        elif check_account_existence.result == False:
            val = False
            msg = "Account %s does not exist!" % account
            lock.release()
            return Bool(val, msg)

        list_user_output = self.list_user(account)

        if list_user_output.val == False:
            val = False
            msg = "Failed to list all users: " + list_user_output.msg
            lock.release()
            return Bool(val, msg)
        else:
            logger.info(list_user_output.msg)
            for field, value in list_user_output.msg.items():
                user_list.append(field)

        for user in user_list:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_user, account=account, user=user)
            if val == False:
                logger.error(msg)
                lock.release()
                return Bool(val, msg)

        metadata_object = account + " " + self.__metadata_name

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_target,\
                                           account=".super_admin", target=metadata_object, admin_user=".super_admin",\
                                           admin_password=self.__password)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_account, account=account)

        if val == False:
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    def enable_user(self, account, user, retry=3):
        '''
        Enable the user by modifying the file "<user name>" of the container "<account name>" in super_admin account.

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
        logger = util.getLogger(name="enable_user")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        user_content = {}
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        user_existence_output = self.user_existence(account, user)

        if user_existence_output.val == False:
            val = False
            msg = user_existence_output.msg
            lock.release()
            return Bool(val, msg)
        elif user_existence_output.result == False:
            val = False
            msg = "User %s:%s does not exist!" % (account, user)
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=user)

        if val == False:
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            user_content = msg
            tmp = user_content.get("disable")

        if tmp == None:
            val = False
            msg = "User %s:%s has been enabled!" % (account, user)
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            user_content["auth"] = tmp
            val = user_content.pop("disable", False)

        if val == False:
            msg = "Failed to modify file %s in container %s!" % (user, account)
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=account, object_name=user, object_content=user_content)

        if val == False:
            logger.error(msg)

        lock.release()
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
        @return: a tuple Bool(val, msg). If the user's password is successfully changed and the original password is
                stored in the metadata container, then Bool.val = True and Bool.msg = the standard output. Otherwise,
                Bool.val == False and Bool.msg indicates the error message.
        '''
        logger = util.getLogger(name="disable_user")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        user_content = {}
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        user_existence_output = self.user_existence(account, user)

        if user_existence_output.val == False:
            val = False
            msg = user_existence_output.msg
            lock.release()
            return Bool(val, msg)
        elif user_existence_output.result == False:
            val = False
            msg = "User %s:%s does not exist!" % (account, user)
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=user)

        if val == False:
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            user_content = msg
            tmp = user_content.get("disable")

        if tmp != None:
            val = False
            msg = "User %s:%s has been disabled!" % (account, user)
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            user_content["disable"] = user_content.get("auth")
            val = user_content.pop("auth", False)

        if val == False:
            msg = "Failed to modify file %s in container %s!" % (user, account)
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=account, object_name=user, object_content=user_content)

        if val == False:
            logger.error(msg)

        lock.release()
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
        logger = util.getLogger(name="enable_account")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        services_content = {}
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        check_account_existence = self.account_existence(account)

        if check_account_existence.val == False:
            val = False
            msg = check_account_existence.msg
            lock.release()
            return Bool(val, msg)
        elif check_account_existence.result == False:
            val = False
            msg = "Account %s does not exist!" % account
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=".services")

        if val == False:
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            services_content = msg
            tmp = services_content.get("disable")

        if tmp == None:
            val = False
            msg = "Account %s has been enabled!" % account
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            services_content["storage"] = tmp
            val = services_content.pop("disable", False)

        if val == False:
            msg = "Failed to modify the file .services!"
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=account, object_name=".services", object_content=services_content)

        if val == False:
            logger.error(msg)

        lock.release()
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
        logger = util.getLogger(name="diable_account")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        services_content = {}
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        check_account_existence = self.account_existence(account)

        if check_account_existence.val == False:
            val = False
            msg = check_account_existence.msg
            lock.release()
            return Bool(val, msg)
        elif check_account_existence.result == False:
            val = False
            msg = "Account %s does not exist!" % account
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=".services")

        if val == False:
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            services_content = msg
            tmp = services_content.get("disable")

        if tmp != None:
            val = False
            msg = "Account %s has been disabled!" % account
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            services_content["disable"] = services_content.get("storage")
            val = services_content.pop("storage", False)

        if val == False:
            msg = "Failed to modify the file .services!"
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=account, object_name=".services", object_content=services_content)

        if val == False:
            logger.error(msg)

        lock.release()
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
        logger = util.getLogger(name="change_password")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        user_info = {}
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        user_existence_output = self.user_existence(account, user)

        if user_existence_output.val == False:
            val = False
            msg = user_existence_output.msg
            lock.release()
            return Bool(val, msg)
        elif user_existence_output.result == False:
            val = False
            msg = "User %s does not exist!" % user
            lock.release()
            return Bool(val, msg)

        if newPassword == "":
            newPassword = self.__generate_random_password()

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=user)

        if val == False:
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            user_info = msg
            user_info["auth"] = "plaintext:" + newPassword

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=user, object_content=user_info)

        if val == False:
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    @util.timeout(300)
    def __get_account_usage(self, proxyIp, account, user):
        '''
        Return the statement of the given user in the give account.
        (Not finished yet)
        '''
        logger = util.getLogger(name="__get_account_usage")

        url = "https://%s:%s/auth/v1.0" % (proxyIp, self.__auth_port)
        password = self.get_user_password(account, user).msg

        cmd = "swift -A %s -U %s:%s -K %s stat" % (url, account, user, password)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = stderrData
            logger.error(msg)
        else:
            val = True
            msg = stdoutData
            logger.info(msg)

        return Bool(val, msg)

    def get_account_usage(self, account, user, retry=3):
        '''
        Get account usage from backend Swift.
        (Not finished yet)

        @type  account: string
        @param account: the account name of the user
        @type  user: string
        @param user: the user to be checked
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If get the account usage successfully, then Bool.val == True, and Bool.msg == "".
                Otherwise, Bool.val == False, and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="get_account_usage")

        #proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        proxy_ip_list = self.__proxy_ip_list
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list,\
                                           retry=retry,\
                                           fn=self.__get_account_usage,\
                                           account=account, user=user)

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
        logger = util.getLogger(name="get_user_password")

        proxy_ip_list = self.__proxy_ip_list
        user_detail = {}
        user_password = ""
        password = ""
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
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

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=user)

        if val == False:
            logger.error(msg)
            return Bool(val, msg)
        else:
            user_detail = msg
            user_password = user_detail["auth"]

        if user_password is None:
            val = False
            msg = "Failed to get password of user %s:%s!" % (account, user)
        else:
            val = True
            logger.info(user_password)
            password = user_password.split(":")
            msg = password[-1]

        return Bool(val, msg)

    def set_account_quota(self, account, admin_container, admin_user, quota, retry=3):
        '''
        Set the quota of the given account by updating the metadata
        in the container for the admin user of the given account.
        (Not finished yet)

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
        @return: a tuple Bool(val, msg). If the account's quota is successfully set, then Bool.val = True and
                Bool.msg = the standard output. Otherwise, Bool.val == False and Bool.msg indicates the error message.
        '''
        logger = util.getLogger(name="set_account_quota")

        #proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        proxy_ip_list = self.__proxy_ip_list
        admin_password = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        #TODO: check whehter the container admin_container is associated
        #with admin_user

        #TODO: check whether the quota is a valid number

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
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
                                           account=account, container=admin_container, admin_user=admin_user,\
                                           admin_password=admin_password, metadata_content=container_metadata)

        return Bool(val, msg)

    def get_account_quota(self, account, admin_container, admin_user, retry=3):
        '''
        Get the quota of the given account by reading the metadata in
        the container for the admin user of the given account.
        (Not finished yet)

        @type  account: string
        @param account: the account to be set quota
        @type  admin_container: string
        @param admin_container: the container for the admin user
        @type  admin_user: string
        @param admin_user: the admin user of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry when fn return False
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the account's quota is successfully got, then Bool.val = True and
                Bool.msg = the quota of the account. Otherwise, Bool.val == False and Bool.msg indicates the error message.
        '''
        logger = util.getLogger(name="get_account_quota")
        #proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        proxy_ip_list = self.__proxy_ip_list
        admin_password = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        #TODO: check whehter the container admin_container is associated
        #with admin_user

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            return Bool(val, msg)

        get_admin_password_output = self.get_user_password(account, admin_user)
        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=admin_container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            return Bool(val, msg)

        elif msg["Quota"].isdigit():
            msg = int(msg["Quota"])

        else:
            val = False
            msg = "The value of the quota in the metadata is not a number."

        return Bool(val, msg)

    def set_user_quota(self, account, container, user, admin_user, quota, retry=3):
        '''
        Set the quota of the given user by updating the metadata in the container for the user.
        (Not finished yet)

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
        @return: a tuple Bool(val, msg). If the user's quota is successfully set, then Bool.val = True and
                Bool.msg = the standard output. Otherwise, Bool.val == False and Bool.msg indicates the error message.
        '''
        logger = util.getLogger(name="set_user_quota")
        #proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
        proxy_ip_list = self.__proxy_ip_list
        admin_password = ""

        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        #TODO: check whehter the container is associated with the given user
        #TODO: check whether the quota is a valid number

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
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

    @util.timeout(300)
    def __get_account_info(self, proxyIp):
        '''
        Get the account information of Swift. The account information is stored in Swauth.

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operation is successfully done, then val == True and msg will
                record the information. Otherwise, val == False and msg will record the error message.
        '''
        logger = util.getLogger(name="__get_account_info")

        url = "https://%s:%s/auth/" % (proxyIp, self.__auth_port)
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
        Check whether the account exists.

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

        proxy_ip_list = self.__proxy_ip_list
        account_info = {}
        result = False
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "result val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(result, val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
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
            if item["name"] == account:
                result = True

        return Bool(result, val, msg)

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
        logger = util.getLogger(name="list_account")

        proxy_ip_list = self.__proxy_ip_list
        account_info = {}
        user_info = {}
        account_dict = {}
        services_content = {}
        metadata_content = {}
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_account_info)

        if val == False:
            logger.error(msg)
            return Bool(val, msg)
        else:
            try:
                account_info = json.loads(msg)
                val = True
                msg = ""
            except Exception as e:
                val = False
                msg = "Failed to load the json string: %s" % str(e)
                logger.error(msg)
                return Bool(val, msg)

        for item in account_info["accounts"]:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_user_info, account=item["name"])

            if val == False:
                user_number = "Error"
                logger.error(msg)
            else:
                try:
                    user_info = json.loads(msg)
                    user_number = len(user_info["users"])
                except Exception as e:
                    msg = "Failed to load the json string: %s" % str(e)
                    logger.error(msg)
                    user_number = "Error"

            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=item["name"], object_name=self.__metadata_name)

            if val == False:
                logger.error(msg)
                mark = False
            else:
                metadata_content = msg
                mark = True

            if metadata_content.get(self.__admin_default_name) != None:
                description = metadata_content.get(self.__admin_default_name).get("description") if mark == True else "Error"
                quota = metadata_content.get(self.__admin_default_name).get("quota") if mark == True else "Error"
            else:
                description = "Error"
                quota = "Error"

            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=item["name"], object_name=".services")

            if val == False:
                account_enable = "Error"
                logger.error(msg)
            else:
                services_content = msg
                account_enable = True if services_content.get("disable") == None else False
            
            account_dict[item["name"]] = {
                "user_number": user_number,
                "description": description,
                "quota": quota,
                "account_enable": account_enable,
            }

        val = True
        msg = account_dict

        return Bool(val, msg)

    @util.timeout(300)
    def __get_container_info(self, proxyIp, account, admin_user, admin_password):
        '''
        Return the container information of the account.

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the account to be queried
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  admin_password: string
        @param admin_password: the password of account administrator
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operation is successfully done, then val == True and msg records
                the container information. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_container_info")

        url = "https://%s:%s/auth/v1.0" % (proxyIp, self.__auth_port)
        msg = "Failed to get the container information of account %s: " % account
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
        List all containers of the account.

        @type  account: string
        @param account: the account name of the user
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the container list is got successfully, then Bool.val == True
                and Bool.msg == container list. Otherwise, Bool.val == False, and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="list_container")

        proxy_ip_list = self.__proxy_ip_list
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            return Bool(val, msg)

        if account != ".super_admin":
            account_existence_output = self.account_existence(account)

            if account_existence_output.val == False:
                val = False
                msg = account_existence_output.msg
                return Bool(val, msg)
            elif account_existence_output.result == False:
                val = False
                msg = "Account %s does not exist!" % account
                return Bool(val, msg)

        if admin_user == ".super_admin":
            admin_password = self.__password
        else:
            get_user_password_output = self.get_user_password(account, admin_user)

            if get_user_password_output.val == True:
                admin_password = get_user_password_output.msg
            else:
                val = False
                msg = get_user_password_output.msg
                return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_info,\
                                           account=account, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)
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
        Return the user's information of the account. The user's information is stored in Swauth.

        @type  proxyIp: string
        @param proxyIp: IP of the proxy node
        @type  account: string
        @param account: the account to be queried
        @rtype:  named tuple
        @return: a tuple Bool(val, msg). If the operation is successfully done, then val == True and msg records the user
                information. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_user_info")

        url = "https://%s:%s/auth/" % (proxyIp, self.__auth_port)
        msg = "Failed to get the user information in account %s: " % account
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
        logger = util.getLogger(name="list_user")

        proxy_ip_list = self.__proxy_ip_list
        user_info = {}
        user_dict = {}
        metadata_content = {}
        user_content = {}
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            return Bool(val, msg)

        account_existence_output = self.account_existence(account)

        if account_existence_output.val == False:
            val = False
            msg = account_existence_output.msg
            return Bool(val, msg)
        elif account_existence_output.result == False:
            val = False
            msg = "Account %s does not exist!" % account
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

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name)

        if val == False:
            logger.error(msg)
            mark = False
        else:
            metadata_content = msg
            mark = True

        for item in user_info["users"]:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=account, object_name=item["name"])

            if val == False:
                user_enable = "Error"
                logger.error(msg)
            else:
                user_content = msg
                user_enable = True if user_content.get("disable") == None else False

            if metadata_content.get(item["name"]) != None:
                description = metadata_content.get(item["name"]).get("description") if mark == True else "Error"
                quota = metadata_content.get(item["name"]).get("quota") if mark == True else "Error"
            else:
                description = "Error"
                quota = "Error"

            user_dict[item["name"]] = {
                "description": description,
                "quota": quota,
                "user_enable": user_enable,
            }

        val = True
        msg = user_dict

        return Bool(val, msg)

    def user_existence(self, account, user, retry=3):
        '''
        Check whether the given user exists in the account.

        @type  account: string
        @param account: the account name to be checked
        @type  user: string
        @param user: the user to be checked
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(result, val, msg). If the user exists, then Bool.result == True, Bool.val == True, and
                Bool.msg == "". If the user does not exist, then Bool.result == False, Bool.val == True, and Bool.msg == "".
                Otherwise, Bool.result == False, Bool.val == False, and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="user_existence")

        proxy_ip_list = self.__proxy_ip_list
        user_info = {}
        result = False
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "result val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(result, val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            return Bool(result, val, msg)

        account_existence_output = self.account_existence(account)

        if account_existence_output.val == False:
            result = False
            val = False
            msg = account_existence_output.msg
            return Bool(result, val, msg)
        elif account_existence_output.result == False:
            result = False
            val = False
            msg = "Account %s does not exist!" % account
            return Bool(result, val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_user_info, account=account)

        if val == False:
            logger.error(msg)
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
        Assign the user of the account to the read acl of the container.

        @type  account: string
        @param account: the account of the user
        @type  container: string
        @param container: the container to assign the read acl
        @type  user: string
        @param user: the user to add into the read acl
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the read acl is successfully assigned, then val == True and msg == "".
                Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="assign_read_acl")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container not in msg:
            val = False
            msg = "Container %s does not exist!" % container
            lock.release()
            return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = get_user_password_output.msg
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            ori_read_acl = msg["Read"]

            if "%s:%s" % (account, user) in ori_read_acl:
                val = True
                msg = ""
                lock.release()
                return Bool(val, msg)
            else:
                msg["Read"] = ori_read_acl + "," + "%s:%s" % (account, user)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_container_metadata,\
                                           account=account, container=container, admin_user=admin_user,\
                                           admin_password=admin_password, metadata_content=msg)

        if val == False:
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    def assign_write_acl(self, account, container, user, admin_user, retry=3):
        '''
        Assign the user of the account to the write acl of the container.

        @type  account: string
        @param account: the account of the user
        @type  container: string
        @param container: the container to assign the write acl
        @type  user: string
        @param user: the user to add into the write acl
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the write acl is successfully assigned, then val == True and msg == "".
                Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="assign_write_acl")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container not in msg:
            val = False
            msg = "Container %s does not exist!" % container
            lock.release()
            return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = get_user_password_output.msg
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            ori_read_acl = msg["Read"]
            ori_write_acl = msg["Write"]

            if "%s:%s" % (account, user) in ori_write_acl and "%s:%s" % (account, user) in ori_read_acl:
                val = True
                msg = ""
                lock.release()
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
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    def remove_read_acl(self, account, container, user, admin_user, retry=3):
        '''
        Remove the user of the account from the read acl of the container.

        @type  account: string
        @param account: the account of the user
        @type  container: string
        @param container: the container to remove the read acl
        @type  user: string
        @param user: the user to remove from the read acl
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the user is successfully removed from the read ACL, then val == True
                and msg == "". Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="remove_read_acl")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        ori_read_acl = ""
        ori_write_acl = ""
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container not in msg:
            val = False
            msg = "Container %s does not exist!" % container
            lock.release()
            return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = get_user_password_output.msg
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)
            lock.release()
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
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    def remove_write_acl(self, account, container, user, admin_user, retry=3):
        '''
        Remove the user of the account from the write acl of the container.

        @type  account: string
        @param account: the account of the user
        @type  container: string
        @param container: the container to remove the write acl
        @type  user: string
        @param user: the user to remove from the write acl
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the user is successfully removed from the write ACL, then val == True
                and msg == "". Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="remove_write_acl")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        ori_write_acl = ""
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found"
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1"
            lock.release()
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container not in msg:
            val = False
            msg = "Container %s does not exist!" % container
            lock.release()
            return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = get_user_password_output.msg
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)
            lock.release()
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
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    @util.timeout(300)
    def __create_container(self, proxyIp, account, container, admin_user, admin_password):
        logger = util.getLogger(name="__create_container")

        url = "https://%s:%s/auth/v1.0" % (proxyIp, self.__auth_port)
        msg = "Fail to create container %s in account %s: " % (container, account)
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
        Create a new container in the account by account administrator.

        @type  account: string
        @param account: the account to create a container
        @type  container: string
        @param container: the container to be created
        @type  admin_user: string
        @param admin_user: account administrator
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the creation is successfully done, then val == True and msg == "".
                Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="create_container")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container in msg:
            val = False
            msg = "Container %s has existed!" % container
            lock.release()
            return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = get_user_password_output.msg
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__create_container, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    @util.timeout(300)
    def __delete_target(self, proxyIp, account, target, admin_user, admin_password):
        logger = util.getLogger(name="__delete_target")

        url = "https://%s:%s/auth/v1.0" % (proxyIp, self.__auth_port)
        msg = "Fail to delete target %s in account %s: " % (target, account)
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s delete %s" % (url, account, admin_user, admin_password, target)
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
        Delete the container in the account by account administrator. 

        @type  account: string
        @param account: the account to delete the container
        @type  container: string
        @param container: the container to be deleted
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the deletion is successfully done, then val == True
                and msg == "". Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="delete_container")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        admin_password = ""
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container not in msg:
            val = False
            msg = "Container %s does not exist!" % container
            lock.release()
            return Bool(val, msg)

        get_user_password_output = self.get_user_password(account, admin_user)

        if get_user_password_output.val == True:
            admin_password = get_user_password_output.msg
        else:
            val = False
            msg = get_user_password_output.msg
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_target, account=account,\
                                           target=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    @util.timeout(300)
    def __set_container_metadata(self, proxyIp, account, container, admin_user, admin_password, metadata_content):
        '''
        Set the metadata of the container with metadata_content.
        
        The following is an example of metadata_content::
            metadata_content = {
                "Read": read ACL list,
                "Write": write ACL list,
                "User-Defined-Metadata-1": xxx,
                "User-Defined-Metadata-2": ooo,
            }

        @type  proxyIp: string
        @param proxyIp: ip of the proxy node
        @type  account: string
        @param account: the account of the container
        @type  container: string
        @param container: the container to be set metadata
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  admin_password: string
        @param admin_password: the password of account administrator
        @type  metadata_content: dictionary
        @param metadata_content: the content to be set to metadata of the container
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the metadata are successfully set, then val == True and msg == "".
                Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__set_container_metadata")

        url = "https://%s:%s/auth/v1.0" % (proxyIp, self.__auth_port)
        msg = "Failed to set the metadata of container %s: " % container
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s post %s" % (url, account, admin_user, admin_password, container)

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
        Get the metadata of the given container as a dictionary, including read/write ACL and other
        user-defined metadata.

        The following is an example of metadata::
            {
                "Read": read ACL list,
                "Write": write ACL list,
                "User-Defined-Metadata-1": xxx,
                "User-Defined-Metadata-2": ooo,
            }

        @type  proxyIp: string
        @param proxyIp: ip of the proxy node
        @type  account: string
        @param account: the account of the container
        @type  container: string
        @param container: the container to get metadata
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  admin_password: string
        @param admin_password: the password of account administrator
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the metadata are successfully got, then val == True and msg
                records the metadata. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_container_metadata")

        url = "https://%s:%s/auth/v1.0" % (proxyIp, self.__auth_port)
        msg = "Failed to get the metadata of container %s: " % container
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
                    metadata_content[line.split()[1][:-1]] = line.split(": ")[1]
            elif "Read" in line:
                metadata_content["Read"] = line.split("ACL: ")[1]
            elif "Write" in line:
                metadata_content["Write"] = line.split("ACL: ")[1]

        val = True
        msg = metadata_content
        logger.info(msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __get_object_content(self, proxyIp, account, container, object_name, admin_user, admin_password):
        '''
        Get the content of the object in the container of the account.

        @type  proxyIp: string
        @param proxyIp: ip of the proxy node
        @type  account: string
        @param account: the account of the container
        @type  container: string
        @param container: the container of the object
        @type  object_name: string
        @param object_name: the name of the object
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  admin_password: string
        @param admin_password: the password of account administrator
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the content is successfully got, then val == True and msg
                records the content. Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_object_content")

        url = "https://%s:%s/auth/v1.0" % (proxyIp, self.__auth_port)
        msg = "Failed to get the content of the object %s: " % object_name
        val = False
        object_content = {}
        Bool = collections.namedtuple("Bool", "val msg")

        cmd = "swift -A %s -U %s:%s -K %s download %s %s -o -" % (url, account, admin_user, admin_password, container, object_name)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
            return Bool(val, msg)
        else:
            line = stdoutData.split("\n")[0]

        try:
            object_content = json.loads(line)
            val = True
            msg = object_content
        except Exception as e:
            val = False
            msg = "Failed to load json string: %s" % str(e)
            logger.error(msg)
            return Bool(val, msg)

        return Bool(val, msg)

    @util.timeout(300)
    def __set_object_content(self, proxyIp, account, container, object_name, admin_user, admin_password, object_content):
        '''
        Set the content of the object in the container of the account.

        @type  proxyIp: string
        @param proxyIp: ip of the proxy node
        @type  account: string
        @param account: the account of the container
        @type  container: string
        @param container: the container of the object
        @type  object_name: string
        @param object_name: the name of the object
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  admin_password: string
        @param admin_password: the password of account administrator
        @type  object_content: dictionary
        @param object_content: the content to be set
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the content is successfully set, then val == True and msg == "".
                Otherwise, val == False and msg records the error message.
        '''
        logger = util.getLogger(name="__get_object_content")

        url = "https://%s:%s/auth/v1.0" % (proxyIp, self.__auth_port)
        msg = "Failed to set the content of the object %s: " % object_name
        val = False
        Bool = collections.namedtuple("Bool", "val msg")
        json_str = ""

        try:
            json_str = json.dumps(object_content)
        except Exception as e:
            val = False
            msg = "Failed to dump into json string: %s" % str(e)
            logger.error(msg)
            return Bool(val, msg)

        os.system("echo \'%s\' >> %s" % (json_str, object_name))

        cmd = "swift -A %s -U %s:%s -K %s upload %s %s" % (url, account, admin_user, admin_password, container, object_name)
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdoutData, stderrData) = po.communicate()

        if po.returncode != 0 or stderrData != "":
            val = False
            msg = msg + stderrData
            logger.error(msg)
        else:
            val = True
            msg = ""

        os.system("rm %s" % object_name)
        return Bool(val, msg)

    def obtain_user_info(self, account, user, retry=3):
        '''
        Obtain the related information of the user in the account.

        @type  account: string
        @param account: the account name of the user
        @type  user: string
        @param user: the user to obtain the related information
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the information of the user is successfully got, then Bool.val == True
                and Bool.msg is a dictionary recording the related information of the user. Otherwise,
                Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="obtain_user_info")

        proxy_ip_list = self.__proxy_ip_list
        user_info = {}
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
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

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name)

        if val == False:
            description = "Error!"
            quota = "Error!"
            logger.error(msg)
        else:
            description = msg.get(user).get("description")
            quota = msg.get(user).get("quota")

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=user)

        if val == False:
            user_enable = "Error"
            logger.error(msg)
        else:
            user_enable = True if msg.get("disable") == None else False

        user_info = {
            "description": description,
            "quota": quota,
            "user_enable": user_enable,
        }

        val = True
        msg = user_info

        return Bool(val, msg)

    def modify_user_description(self, account, user, description, retry=3):
        '''
        Modify the description of the user's metadata stored in super_admin account.

        @type  account: string
        @param account: the account name of the user
        @type  user: string
        @param user: the user to modify the description
        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the description of the user is successfully modified, then Bool.val == True
                and Bool.msg == "". Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="obtain_user_info")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        metadata_content = {}
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        if len(description.split()) == 0:
            msg = "Description can not be an empty string."
            lock.release()
            return Bool(val, msg)

        user_existence_output = self.user_existence(account, user)

        if user_existence_output.val == False:
            val = False
            msg = user_existence_output.msg
            lock.release()
            return Bool(val, msg)
        elif user_existence_output.result == False:
            val = False
            msg = "User %s:%s does not exist!" % (account, user)
            lock.release()
            return Bool(val, msg)

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name)

        if val == False:
            msg = "Failed to get the metadata: " + msg
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            metadata_content = msg
            metadata_content[user]["description"] = description

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name, object_content=metadata_content)

        if val == False:
            msg = "Failed to modify the description of user %s:%s" % (account, user) + msg
            logger.error(msg)

        lock.release()
        return Bool(val, msg)


if __name__ == '__main__':
    SA = SwiftAccountMgr()
    #print SA.add_account("test1")
    #print SA.add_user("test1", "user1")
    #print SA.delete_account("test1")
    #print SA.delete_user("test1", "user1")
    #print SA.enable_account("account1")
    #print SA.disable_account("account1")
    #print SA.enable_user("account1", "user0")
    #print SA.disable_user("account1", "user0")
    #print SA.obtain_user_info("account0", "user3")
    #print SA.modify_user_description("account0", "user3", "   This is very good!!!!")
    #print SA.list_user("test1")
    #print SA.list_account()
