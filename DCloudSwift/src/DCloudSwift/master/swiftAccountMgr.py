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
sys.path.insert(0,"%s/DCloudSwift/" % BASEDIR)
#sys.path.append("%s/DCloudSwift/" % BASEDIR)

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
        logger = util.getLogger(name="add_user")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        private_container = user + self.__private_container_suffix
        config_container = user + self.__config_container_suffix
        metadata_content = {}
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        # the password of the user will be randomly assigned when the password is empty
        if password == "":
            password = self.__generate_random_password()

        if len(description.split()) == 0:
            msg = "Description can not be an empty string."
            lock.release()
            return Bool(val, msg)

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            lock.release()
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            lock.release()
            return Bool(val, msg)

        # check the existence of the account
        # for account administrator and general users, the handling is different
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

        # prepare the metadata
        # for account administrator and general users, the handling is different
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

        # set the metadata
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name, object_content=metadata_content)

        # create the private container for general users
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

        # create the gateway configuration container for general users
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

        # actually create the user by Swauth CLI
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

        # get the password of account administrator
        # the password will be used to list all containers of the account
        get_admin_password_output = self.get_user_password(account, self.__admin_default_name)

        if get_admin_password_output.val == False:
            val = False
            msg = get_admin_password_output.msg
            lock.release()
            return Bool(val, msg)
        else:
            admin_password = get_admin_password_output.msg

        # obtain the metadata and remove the related field of the user
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

        # set the modified metadata
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name, object_content=metadata_content)

        if val == False:
            msg = "Failed to delete the metadata: " + msg
            logger.error(msg)
            lock.release()
            return Bool(val, msg)

        # list all containers of the account by account administrator 
        account_admin_container = self.list_container(account, self.__admin_default_name)

        if account_admin_container.val == False:
            val = False
            msg = account_admin_container.msg
            lock.release()
            return Bool(val, msg)

        # check the existence of the user's private container and then remove it
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

        # check the existence of the user's gateway configuration container and then remove it            
        if config_container in account_admin_container.msg:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_target,\
                                               account=account, target=config_container, admin_user=self.__admin_default_name,\
                                               admin_password=admin_password)
        else:
            val = True
            msg = ""

        # actually delete the user by Swauth CLI
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

    def add_account(self, account, admin_user="", admin_password="", description="no description", quota=0, retry=3):
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
        logger = util.getLogger(name="add_account")

        lock.acquire()
        proxy_ip_list = self.__proxy_ip_list
        msg = ""
        val = False
        Bool = collections.namedtuple("Bool", "val msg")

        # in fact, the name of account administrator can be assigned
        # however, this is not recommended
        admin_user = self.__admin_default_name

        # the password of account administrator will be randomly assigned
        # when the password is empty
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

        # create the account and account administrator
        add_user_output = self.add_user(account=account, user=self.__admin_default_name, password=admin_password,\
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

        # obtain the user list of the account
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
                lock.release()
                return Bool(val, msg)

        for item in user_info["users"]:
            user_list.append(item["name"])

        # delete all users by Swauth CLI
        for user in user_list:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_user, account=account, user=user)
            if val == False:
                logger.error(msg)
                lock.release()
                return Bool(val, msg)

        # delete the metadata and the file .services
        metadata_object = account + " " + self.__metadata_name + " " + ".services"

        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_target,\
                                           account=".super_admin", target=metadata_object, admin_user=".super_admin",\
                                           admin_password=self.__password)

        # delete the accout by Swauth CLI
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_account, account=account)

        if val == False:
            logger.error(msg)

        lock.release()
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

        # obtain the file <user name> in the container <account> of super_admin account and modify the file
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

        # upload the modified file to super_admin account
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
        @return: a tuple Bool(val, msg). If the user is successfully disabled, then Bool.val = True and
                Bool.msg = the standard output. Otherwise, Bool.val == False and Bool.msg indicates the error message.
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

        # obtain the file <user name> in the container <account> of super_admin account and modify it
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

        # upload the modified file to super_admin account
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

        # obtain the file .services in the container <account> of super_admin account and modify it
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

        # upload the file .services to super_admin account
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

        # obtain the file .services in the container <account> of super_admin account and modify it
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

        # upload the file .services to super_admin account
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

        # obtain the file <user name> in the container <account> of super_admin account and modify it
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

        # upload the file <user name> to super_admin account
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=user, object_content=user_info)

        if val == False:
            logger.error(msg)

        lock.release()
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

        # download the file <user name> in the container <account> of super_admin account to obtain the user password
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
        logger = util.getLogger(name="set_account_quota")

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

        # TODO: need to check the value of quota more precisely
        if not str(quota).isdigit():
            msg = "Quota must be a positive integer."
            lock.release()
            return Bool(val, msg)

        user_existence_output = self.user_existence(account, self.__admin_default_name)

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

        # obtain the file .metadata in the container <account name> of super_admin account to get the original quota
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
            ori_quota = metadata_content[self.__admin_default_name]["quota"]

        # must compare the new quota with the original quota to check the validity of the new quota
        if ori_quota == quota:
            val = True
            msg = ""
        else:
            if ori_quota > quota:
                # TODO: the following invocation should be modified to seed up
                obtain_account_info_output = self.obtain_account_info(account)

                if obtain_account_info_output.val == False:
                    val = False
                    msg = "Failed to modify the quota of account %s: " % account + obtain_account_info_output.msg
                    lock.release()
                    return Bool(val, msg)
                else:
                    account_usage = int(obtain_account_info_output.msg.get("usage"))

                if quota < account_usage:
                    val = False
                    msg = "Failed to modify the quota of account %s: usage %d is larger than quota %d" % (account, account_usage, quota)
                    logger.error(msg)
                    lock.release()
                    return Bool(val, msg)
                else:
                    metadata_content[self.__admin_default_name]["quota"] = quota
            else:
                metadata_content[self.__admin_default_name]["quota"] = quota

            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=account, object_name=self.__metadata_name, object_content=metadata_content)

        if val == False:
            msg = "Failed to modify the quota of account %s: " % account + msg
            logger.error(msg)

        lock.release()
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
        logger = util.getLogger(name="set_user_quota")

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

        # TODO: need to check the value of quota more precisely
        if not str(quota).isdigit():
            msg = "Quota must be a positive integer."
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
            ori_quota = metadata_content[user]["quota"]

        # obtain the account quota and the sum of other users' quotas
        '''
        total_quota = 0
        account_quota = 0
        for field, value in metadata_content.items():
            if field != self.__admin_default_name:
                total_quota += value["quota"]
            else:
                account_quota = value["quota"]
        '''

        # must compare the new quota with the original quota to check the validity of the new quota
        if ori_quota == quota:
            val = True
            msg = ""
        elif ori_quota > quota:
            # TODO: the following invocation should be modified to seed up
            obtain_user_info_output = self.obtain_user_info(account, user)

            if obtain_user_info_output.val == False:
                val = False
                msg = "Failed to modify the quota of user %s:%s: " % (account, user) + obtain_account_info_output.msg
                logger.error(msg)
                lock.release()
                return Bool(val, msg)
            elif str(obtain_user_info_output.msg.get("usage")).isdigit():
                user_usage = int(obtain_user_info_output.msg.get("usage"))
            else:
                val = False
                msg = "Failed to modify the quota of user %s:%s: can not obtain the usage of the user."% (account, user)
                logger.error(msg)
                lock.release()
                return Bool(val, msg)

            if quota < user_usage:
                val = False
                msg = "Failed to modify the quota of user %s:%s: usage %d is larger than quota %d." % (account, user, user_usage, quota)
                logger.error(msg)
                lock.release()
                return Bool(val, msg)
            else:
                metadata_content[user]["quota"] = quota
        else:
            metadata_content[user]["quota"] = quota
            '''
            if account_quota >= (total_quota + (quota - ori_quota)):
                metadata_content[user]["quota"] = quota
            else:
                val = False
                msg = "Failed to modify the quota of user %s:%s: exceed the account quota." % (account, user)
                logger.error(msg)
                lock.release()
                return Bool(val, msg)
            '''

        if ori_quota != quota:
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=account, object_name=self.__metadata_name, object_content=metadata_content)

        if val == False:
            msg = "Failed to modify the quota of user %s:%s: " % (account, user) + msg
            logger.error(msg)

        lock.release()
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
        @param account: the account name to be queried
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

        # obtain the account info by Swauth CLI
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
        logger = util.getLogger(name="list_account")

        proxy_ip_list = self.__proxy_ip_list
        account_info = {}
        account_dict = {}
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            return Bool(val, msg)

        # obtain the account list
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

        # invokde obtain_account_info() for each account
        for item in account_info["accounts"]:
            obtain_account_info_output = self.obtain_account_info(item["name"])

            if obtain_account_info_output.val == False:
                account_dict[item["name"]] = {
                    "user_number": "Error",
                    "description": "Error",
                    "quota": "Error",
                    "account_enable": "Error",
                    "usage": "Error",
                }
            else:
                account_dict[item["name"]] = obtain_account_info_output.msg

        # MUST return true to show information on GUI
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
        @param account: the account name to list all containers
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

        # the method of obtaining the passwords of super_admin and other users is different
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

        # obtain the container list
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
        Return the user's information of the account stored in Swauth.

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

        # check the existence of the account
        account_existence_output = self.account_existence(account)

        if account_existence_output.val == False:
            val = False
            msg = account_existence_output.msg
            return Bool(val, msg)
        elif account_existence_output.result == False:
            val = False
            msg = "Account %s does not exist!" % account
            return Bool(val, msg)

        # obtain the user list in the account
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

        # check the file .services in the container <account> of super_admin account to
        # verify whether the account is enabled or not
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=".services")

        if val == False:
            account_enable = "Error"
            logger.error(msg)
        else:
            services_content = msg
            account_enable = True if services_content.get("disable") == None else False

        get_user_password_output = self.get_user_password(account, self.__admin_default_name)
        user_password = get_user_password_output.msg if get_user_password_output.val == True else None

        # invoke obtain_user_info() to obtain each user's info
        for item in user_info["users"]:
            '''
            obtain_user_info_output = self.obtain_user_info(account, item["name"], account_enable)

            if obtain_user_info_output.val == False:
                user_dict[item["name"]] = {
                    "description": "Error",
                    "quota": "Error",
                    "user_enable": "Error",
                    "usage": "Error",
                }

                logger.error(obtain_user_info_output.msg)
            else:
                user_dict[item["name"]] = obtain_user_info_output.msg
            '''

            # check whether the user is enabled or not by identifying the existence of the field "disable"
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=account, object_name=item["name"])

            if val == False:
                user_enable = "Error"
                logger.error(msg)
            else:
                user_enable = True if msg.get("disable") == None else False

            if mark == True and metadata_content.get(item["name"]) != None:
                description = metadata_content[item["name"]].get("description")
                quota = metadata_content[item["name"]].get("quota")
            else:
                description = "Error"
                quota = "Error"

            # the usage of all users can not be obtained when the account is disabled
            if account_enable == True and user_password != None and item["name"] != self.__admin_default_name:
                # the usage of the user's private container
                (val1, msg1) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                                     account=account, container=item["name"] + self.__private_container_suffix,\
                                                     admin_user=self.__admin_default_name, admin_password=user_password)

                # the usage of the user's gateway configuration container
                (val2, msg2) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                                     account=account, container=item["name"] + self.__config_container_suffix,\
                                                     admin_user=self.__admin_default_name, admin_password=user_password)

                if val1 == False or val2 == False:
                    usage = "Error"
                    logger.error(msg1 + msg2)
                elif msg1.get("Bytes") != None and msg2.get("Bytes") != None:
                    usage = int(msg1.get("Bytes")) + int(msg2.get("Bytes"))
                else:
                    usage = "Error"
            elif account_enable == True and user_password != None and item["name"] == self.__admin_default_name:
                usage = 0
            else:
                usage = "Error"
                msg = "Account %s has been disabled or the password of account administrator cannot be obtained." % account
                logger.error(msg)

            user_dict[item["name"]] = {
                "description": description,
                "quota": quota,
                "user_enable": user_enable,
                "usage": usage,
            }

        # obtain the actual usage of account administrator
        '''
        list_container_output = self.list_container(account, self.__admin_default_name)

        if list_container_output.val == False:
            account_usage = "Error"
        else:
            account_usage = 0
            get_admin_password_output = self.get_user_password(account, self.__admin_default_name)

            if get_admin_password_output.val == False:
                account_usage = "Error"
            else:
                admin_password = get_admin_password_output.msg

            for container in list_container_output.msg:
                (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                                     account=account, container=container,\
                                                     admin_user=self.__admin_default_name, admin_password=admin_password)
                if val == False:
                    account_usage = "Error"
                    break
                else:
                    account_usage += msg["Bytes"]

        if account_usage != "Error":
            for field, value in user_dict.items():
                if str(value["usage"]).isdigit():
                    account_usage -= int(value["usage"])
                else:
                    account_usage = "Error"
                    break

        user_dict[self.__admin_default_name]["usage"] = account_usage
        '''

        # MUST return true to show information on GUI
        val = True
        msg = user_dict

        return Bool(val, msg)

    def user_existence(self, account, user, retry=3):
        '''
        Check whether the user exists in the account.

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

        # obtain the user list of the account
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

        # obtain the container list of the account and check whether the existence of the container
        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container not in list_container_output.msg:
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

        # get the original read ACL of the container and then add the user into the ACL
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            ori_read_acl = msg["Read"]

            # check the repeated occurrence
            if "%s:%s" % (account, user) in ori_read_acl:
                val = True
                msg = ""
                lock.release()
                return Bool(val, msg)
            else:
                msg["Read"] = ori_read_acl + "," + "%s:%s" % (account, user)

        # set the read ACL
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

        # obtain the container list of the account and check the existence of the container
        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container not in list_container_output.msg:
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

        # get the original read ACL and write ACL and then add the user into the read ACL and write ACL
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)
            lock.release()
            return Bool(val, msg)
        else:
            ori_read_acl = msg["Read"]
            ori_write_acl = msg["Write"]

            # check the repeated occurrence
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

        # set the read ACL and write ACL
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

        # obtain the container list of the account and check the existence of the container
        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container not in list_container_output.msg:
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

        # obtain the read ACL and write ACL and then remove the user from the read ACL and write ACL
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

        # set the read ACL and write ACL
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

        # obtain the container list of the account and then check the existence of the container
        list_container_output = self.list_container(account, admin_user)

        if list_container_output.val == False:
            val = False
            msg = list_container_output.msg
            lock.release()
            return Bool(val, msg)
        elif container not in list_container_output.msg:
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

        # obtain the write ACL and then remove the user from the write ACL
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

        # set the write ACL
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

        # obtain the container list of the account and check the existence of the container
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

        # create the container
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__create_container, account=account,\
                                           container=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    @util.timeout(300)
    def __delete_target(self, proxyIp, account, target, admin_user, admin_password):
        '''
        Delete a container or an object from the account.

        @type  proxyIp: string
        @param proxyIp: ip of the proxy node
        @type  account: string
        @param account: the account having the target
        @type  target: string
        @param target: the container or the object to be deleted
        @type  admin_user: string
        @param admin_user: account administrator of the account
        @type  admin_password: string
        @param admin_password: the password of account administrator
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the container or the object is successfully deleted, then Bool.val == True
                and Bool.msg == "". Otherwise, Bool.val == False and Bool.msg records the error message.
        '''
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

        # obtain the container list of the account and check the existence of the container
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

        # delete the container
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_target, account=account,\
                                           target=container, admin_user=admin_user, admin_password=admin_password)

        if val == False:
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    @util.timeout(300)
    def __set_container_metadata(self, proxyIp, account, container, admin_user, admin_password, metadata_content):
        '''
        Set the metadata of the container with metadata_content. The metadata that the function being able to set
        include the read ACL, the write ACL, and other user-defined metadata.
        
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
            elif field != "Bytes":
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
        Get the metadata of the given container as a dictionary, including read/write ACL, the number of bytes,
        and other user-defined metadata.

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
            elif "Bytes" in line:
                metadata_content["Bytes"] = int(line.split(": ")[1])

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
        logger = util.getLogger(name="obtain_account_info")

        proxy_ip_list = self.__proxy_ip_list
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

        # obtain the user list to compute the number of users in the account
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_user_info, account=account)

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

        # obtain the file .metadata in the container <account> of super_admin account to
        # get the description and quota of the account
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name)

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

        # check the file .services in the container <account> of super_admin account to
        # verify whether the account is enabled or not
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=".services")

        if val == False:
            account_enable = "Error"
            logger.error(msg)
        else:
            services_content = msg
            account_enable = True if services_content.get("disable") == None else False

        # obtain the usage of the account
        if account_enable == True:
            get_admin_password_output = self.get_user_password(account, self.__admin_default_name)

            if get_admin_password_output.val == False:
                val = False
                msg = get_admin_password_output.msg
            else:
                admin_password = get_admin_password_output.msg

                (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                                   account=account, container="  ", admin_user=self.__admin_default_name,\
                                                   admin_password=admin_password)
        else:
            val = False
            msg = "Account %s has been disabled!" % account

        if val == False:
            usage = "Error"
            logger.error(msg)
        else:
            usage = msg.get("Bytes") if msg.get("Bytes") != None else "Error"
          
        account_dict = {
            "user_number": user_number,
            "description": description,
            "quota": quota,
            "account_enable": account_enable,
            "usage": usage,
        }

        # MUST return true to show information on GUI
        val = True
        msg = account_dict

        return Bool(val, msg)

    def obtain_user_info(self, account, user, account_enable=True, retry=3):
        '''
        Obtain the related information of the user in the account, including::
            (1) the description of the user
            (2) the quota of the user
            (3) the usgae of the user
            (4) the user is enabled or not

        Note that the function can not obtain the usage of account administrator.

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

        # description and quota can be got from the file .metadat in the container <account> of super_admin account
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

        # check whether the user is enabled or not by identifying the existence of the field "disable"
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=user)

        if val == False:
            user_enable = "Error"
            logger.error(msg)
        else:
            user_enable = True if msg.get("disable") == None else False

        get_user_password_output = self.get_user_password(account, self.__admin_default_name)
        user_password = get_user_password_output.msg if get_user_password_output.val == True else None

        # the usage of all users can not be obtained when the account is disabled
        if account_enable == True and user_password != None and user != self.__admin_default_name:
            # the usage of the user's private container
            (val1, msg1) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                                 account=account, container=user + self.__private_container_suffix,\
                                                 admin_user=self.__admin_default_name, admin_password=user_password)

            # the usage of the user's gateway configuration container
            (val2, msg2) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                                 account=account, container=user + self.__config_container_suffix,\
                                                 admin_user=self.__admin_default_name, admin_password=user_password)

            if val1 == False or val2 == False:
                usage = "Error"
                logger.error(msg1 + msg2)
            elif msg1.get("Bytes") != None and msg2.get("Bytes") != None:
                usage = int(msg1.get("Bytes")) + int(msg2.get("Bytes"))
            else:
                usage = "Error"
        elif account_enable == True and user_password != None and user == self.__admin_default_name:
            usage = 0
        else:
            usage = "Error"
            msg = "Account %s has been disabled or the password of account administrator cannot be obtained." % account
            logger.error(msg)

        user_info = {
            "description": description,
            "quota": quota,
            "user_enable": user_enable,
            "usage": usage,
        }

        # MUST return true to show information on GUI
        val = True
        msg = user_info

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
        logger = util.getLogger(name="modify_user_description")

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

        # modify the file .metadata in the container <account> of super_admin account to change the description of the user
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

        # upload the file .metadata to the container <account> of super_admin account
        (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__set_object_content,\
                                           account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                           container=account, object_name=self.__metadata_name, object_content=metadata_content)

        if val == False:
            msg = "Failed to modify the description of user %s:%s" % (account, user) + msg
            logger.error(msg)

        lock.release()
        return Bool(val, msg)

    def list_usage(self, retry=3):
        '''
        List all usages of all users in all accounts.

        @type  retry: integer
        @param retry: the maximum number of times to retry after the failure
        @rtype:  named tuple
        @return: a named tuple Bool(val, msg). If the information is listed successfully, then Bool.val == True
                and Bool.msg is a dictoinary recording all usages of all users in all accounts. Otherwise,
                Bool.val == False and Bool.msg records the error message.
        '''
        logger = util.getLogger(name="list_usage")

        proxy_ip_list = self.__proxy_ip_list
        account_info = {}
        user_info = {}
        services_content = {}
        usage_dict = {}
        val = False
        msg = ""
        Bool = collections.namedtuple("Bool", "val msg")

        if proxy_ip_list is None or len(proxy_ip_list) == 0:
            msg = "No proxy node is found."
            return Bool(val, msg)

        if retry < 1:
            msg = "Argument retry has to >= 1."
            return Bool(val, msg)

        # obtain the account list
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

        # compute the usages of all users of each accout
        for item in account_info["accounts"]:
            usage_dict[item["name"]] = {}
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_user_info, account=item["name"])

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

            # check the file .services in the container <account> of super_admin account to
            # verify whether the account is enabled or not
            (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_object_content,\
                                               account=".super_admin", admin_user=".super_admin", admin_password=self.__password,\
                                               container=item["name"], object_name=".services")

            if val == False:
                account_enable = "Error"
                logger.error(msg)
            else:
                services_content = msg
                account_enable = True if services_content.get("disable") == None else False

            get_user_password_output = self.get_user_password(item["name"], self.__admin_default_name)
            user_password = get_user_password_output.msg if get_user_password_output.val == True else None

            for user in user_info["users"]: 
                # the usage of all users can not be obtained when the account is disabled
                if account_enable == True and user_password != None and user["name"] != self.__admin_default_name:
                    # the usage of the user's private container
                    (val1, msg1) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                                         account=item["name"], container=user["name"] + self.__private_container_suffix,\
                                                         admin_user=self.__admin_default_name, admin_password=user_password)

                    # the usage of the user's gateway configuration container
                    (val2, msg2) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_container_metadata,\
                                                         account=item["name"], container=user["name"] + self.__config_container_suffix,\
                                                         admin_user=self.__admin_default_name, admin_password=user_password)

                    if val1 == False or val2 == False:
                        usage = "Error"
                        logger.error(msg1 + msg2)
                    elif msg1.get("Bytes") != None and msg2.get("Bytes") != None:
                        usage = int(msg1.get("Bytes")) + int(msg2.get("Bytes"))
                    else:
                        usage = "Error"
                elif account_enable == True and user_password != None and user["name"] == self.__admin_default_name:
                    usage = 0
                else:
                    usage = "Error"
                    msg = "Account %s has been disabled or the password of account administrator cannot be obtained." % item["name"]
                    logger.error(msg)

                usage_dict[item["name"]][user["name"]] = {
                    "usage": usage,
                }

        # MUST return true to show information on GUI
        val = True
        msg = usage_dict

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
