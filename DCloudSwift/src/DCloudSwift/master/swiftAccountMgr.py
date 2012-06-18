import os, sys, socket
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
sys.path.append("%s/DCloudSwift/"%BASEDIR)

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
			cmd = "cp %s %s"%(conf, GlobalVar.SWIFTCONF)
			os.system(cmd)
		else:
			msg = "Confing %s does not exist"%conf
			print >> sys.stderr, msg
			logger.warn(msg)

		if not os.path.isfile(GlobalVar.SWIFTCONF):
			msg ="Config %s does not exist"%GlobalVar.SWIFTCONF
			print >> sys.stderr, msg
			logger.error(msg)
			sys.exit(1)


		self.__deltaDir = GlobalVar.DELTADIR
		self.__swiftDir = self.__deltaDir+"/swift"

		self.__accountDb = AccountDatabaseBroker(GlobalVar.ACCOUNT_DB)
		if not os.path.exists(GlobalVar.ACCOUNT_DB):
			self.__accountDb.initialize()

		self.__SC = SwiftCfg(GlobalVar.SWIFTCONF)
		self.__kwparams = self.__SC.getKwparams()
		self.__password = self.__kwparams['password']

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	def __functionBroker(self, proxy_ip_list, retry, fn, **kwargs):
		'''
		Repeat at most retry times:
		   1. Execute the private function fn with a randomly chosen proxy node and kwargs as input.
		   2. Break if fn retrun True
    		:param proxy_ip_list: ip list of proxy nodes
    		:param retry: retry how many times when fn return False
    		:param fn: private function to call
    		:param kwargs: keyword arguments to fn
		'''
		val = False
		msg =""
		for t in range(retry):
			ip = random.choice(proxy_ip_list)
			try:
				output = fn(ip, **kwargs)
				if output.val == True:
					val = True
					msg = output.msg
					break
				else:
					errMsg = "Failed to run %s thru %s for %s"%(fn.__name__, ip, output.msg)
					msg = msg + '\n' + errMsg
					
			except util.TimeoutError:
				errMsg = "Failed to add account %s thru %s in time"%(account, ip)
				logger.error(errMsg)
				msg = msg + '\n' +errMsg

		return (val, msg)

	@util.timeout(300)
	def __add_user(self, proxyIp, account, name, password, admin=True, reseller=False):
		logger = util.getLogger(name="__add_user")
		self.__class__.__add_user.__name__
		
		url = "https://%s:8080/auth/"%proxyIp
		msg = ""
		val = False

		admin_opt = "-a " if admin else ""
		reseller_opt = "-r " if reseller  else ""
		optStr = admin_opt+reseller_opt

		cmd = "swauth-add-user -K %s -A %s %s %s %s %s"%(self.__password, url, optStr, account, name, password)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()

		if po.returncode !=0:
			logger.error(stderrData)
			msg = stderrData
			val =False
		else:
			logger.info(stdoutData)
			msg = stdoutData
			val =True

		Bool = collections.namedtuple("Bool", "val msg")
                return Bool(val, msg)

	def add_user(self, account, name, password, admin=True, reseller=False, retry=3):
		'''
		add user to the database and backend swift
    		:param account: account of the user
    		:param name: name of the user
    		:param admin: a boolean variable indicates if the user is a admin
    		:param reseller: a boolean variable indicates if the user is reseller
		:param retry: retry how many times when failures
		:returns: return a Bool object. If the user is successfully added to both the database and backend swift
                          then Bool.val == True. Otherwise, Bool.val == False and Bool.msg indicates the reason of failure.

		'''
		logger = util.getLogger(name="add_user")
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		try:
			row = self.__accountDb.add_user(account=account, name=name)

			if row is None:
				msg = "User %s:%s alread exists"%(account, name)
				return Bool(val, msg)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			msg = str(e)
			return Bool(val, msg)
		
		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__add_user,
                                                   account=account, name=name, password=password,
						   admin=admin, reseller=reseller)

		try:
			if val == False:
				self.__accountDb.delete_user(account=account, name=name)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to clean user %s:%s from database for %s"%(account, name, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	@util.timeout(300)
	def __delete_user(self, proxyIp, account, name):
		logger = util.getLogger(name="__delete_user")

		url = "https://%s:8080/auth/"%proxyIp
		cmd = "swauth-delete-user -K %s -A %s %s %s"%(self.__password, url, account, name)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()
				
		msg = ""
		val = False

		if po.returncode !=0 and '404' not in stderrData:
			logger.error(stderrData)
			msg = stderrData
			val =False
		elif '404' in stderrData:
			msg = "user %s:%s does not exists"%(account, name)
			logger.warn(msg)
			val =True
		else:
			logger.info(stdoutData)
			msg = stdoutData
			val =True

		Bool = collections.namedtuple("Bool", "val msg")
                return Bool(val, msg)

	def delete_user(self, account, name, retry=3):
		'''
		delte user from the database and backend swift
    		:param account: account of the user
    		:param name: name of the user
		:param retry: retry how many times when the operation failed
		:returns: return a Bool object. If the user is successfully deleted from  both the database and backend swift
                          then Bool.val == True. Otherwise, Bool.val == False and Bool.msg indicates the reason of failure.

		'''
		logger = util.getLogger(name="delete_user")
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list)==0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__delete_user,
                                                   account=account, name=name)

		try:
			if val == True:
				self.__accountDb.delete_user(account=account, name=name)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to clean user %s:%s from database for %s"%(account, name, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	@util.timeout(300)
	def __disable_user(self, proxyIp, account, name):
		logger = util.getLogger(name="__disable_user")

		url = "https://%s:8080/auth/"%proxyIp
		randomPassword = str(uuid.uuid4())
		cmd = "swauth-add-user -K %s -A %s %s %s %s"%(self.__password, url, account, name, randomPassword)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()
				
		msg = ""
		val = False

		if po.returncode !=0:
			logger.error(stderrData)
			msg = stderrData
			val =False
		else:
			logger.info(stdoutData)
			msg = stdoutData
			val =True

		Bool = collections.namedtuple("Bool", "val msg")
                return Bool(val, msg)

	def disable_user(self, account, name, retry=3):
		'''
		Disabe the user from accessing the backend swift by changing
                the user's password in the backend.

    		:param account: account of the user
    		:param name: name of the user
		:param retry: retry how many times when the operation failed
		:returns: return a Bool object. If the user's backend password is successfully changed
                          and the enabled field in the database is set to false then Bool.val == True. 
			  Otherwise, Bool.val == False and Bool.msg indicates the reason of failure.

		'''
		logger = util.getLogger(name="disable_user")
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		
		Bool = collections.namedtuple("Bool", "val msg")
		msg = ""
		val = False

		if proxy_ip_list is None or len(proxy_ip_list)==0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__disable_user,
                                                   account=account, name=name)

		try:
			if val == True:
				self.__accountDb.disable_user(account=account, name=name)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to set enabled=False for user %s:%s in database for %s"%(account, name, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	@util.timeout(300)
	def __enable_user(self, proxyIp, account, name):
		logger = util.getLogger(name="__enable_user")

		url = "https://%s:8080/auth/"%proxyIp
		msg = ""
		val = False

		password = self.__accountDb.get_password(account, name)
		if password is None:
			msg = "user %s:%s does not exists"%(account,name)
			return Bool(val, msg)
		
		admin_opt = "-a " if self.__accountDb.is_admin(account, name) else ""
		reseller_opt = "-r " if self.__accountDb.is_reseller(account, name)  else ""
		optStr = admin_opt + reseller_opt

		cmd = "swauth-add-user -K %s -A %s %s %s %s %s"%(self.__password, url, optStr, account, name, password)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()
				
		if po.returncode !=0:
			logger.error(stderrData)
			msg = stderrData
			val =False
		else:
			logger.info(stdoutData)
			msg = stdoutData
			val =True

		Bool = collections.namedtuple("Bool", "val msg")
                return Bool(val, msg)

	def enable_user(self, account, name, retry=3):
		'''
		Enable the user to access the backend swift by re-adding it to the backend 
                using original setting stored in database.

    		:param account: account of the user
    		:param name: name of the user
		:param retry: retry how many times when the operation failed
		:returns: return a Bool object. If the user is re-added to the backend using the original
                          setting stored in the backend.
			  Otherwise, Bool.val == False and Bool.msg indicates the reason of failure.

		'''
		logger = util.getLogger(name="enable_user")
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list)==0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__enable_user,
                                                   account=account, name=name)
		try:
			if val == True:
				self.__accountDb.enable_user(account=account, name=name)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to set enabled=True for user %s:%s in database for %s"%(account, name, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	@util.timeout(300)
	def __change_password(self, proxyIp, account, user, newPassword, admin=True, reseller=False):
		logger = util.getLogger(name="__change_password")
		#self.__class__.__add_user.__name__

                url = "https://%s:8080/auth/"%proxyIp
                msg = ""
                val = False

                admin_opt = "-a " if admin else ""
                reseller_opt = "-r " if reseller  else ""
                optStr = admin_opt+reseller_opt

                cmd = "swauth-add-user -K %s -A %s %s %s %s %s"%(self.__password, url, optStr, account, name, password)
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                (stdoutData, stderrData) = po.communicate()

                if po.returncode !=0:
                        logger.error(stderrData)
                        msg = stderrData
                        val =False
                else:
                        logger.info(stdoutData)
                        msg = stdoutData
                        val =True

                Bool = collections.namedtuple("Bool", "val msg")
                return Bool(val, msg)

	def change_password(self, account, user, oldPassword, newPassword, retry=3):
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
		@return: a named tuple Bool(val, msg). If the password is changed successfully,
			then Bool.val == True and msg == "". Otherwise, Bool.val == False and
			Bool.msg records the error message.
		'''
		logger = util.getLogger(name="change_password")
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)

		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		'''
		try:
                        if row is None:
                                msg = "User %s:%s alread exists"%(account, name)
                                return Bool(val, msg)

                except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
                        msg = str(e)
                        return Bool(val, msg)
		'''

                (val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__add_user,
                                                   account=account, name=name, password=password,
                                                   admin=admin, reseller=reseller)

                try:
                        if val == False:
                                self.__accountDb.delete_user(account=account, name=name)

                except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
                        errMsg = "Failed to clean user %s:%s from database for %s"%(account, name, str(e))
                        logger.error(errMsg)
                        raise InconsistentDatabaseError(errMsg)

		return Bool(val, msg)

	def add_account(self, account):
		pass

	def delete_account(self, account):
		pass
	
	def list_account(self, account):
		pass

	def list_user(self, account):
		pass

	@util.timeout(300)	
	def __get_account_usage(self, proxyIp, account, name):
		logger = util.getLogger(name="__get_account_user")

		url = "https://%s:8080/auth/v1.0"%proxyIp
		password = self.get_user_password(account, name).msg
		cmd = "swift -A %s -U %s:%s -K %s stat"%(url, account, name, password)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()
				
		msg = ""
		val = False

		if po.returncode !=0:
			logger.error(stderrData)
			msg = stderrData
			val =False
		else:
			logger.info(stdoutData)
			msg = stdoutData
			val =True

		Bool = collections.namedtuple("Bool", "val msg")
                return Bool(val, msg)

	def get_account_usage(self, account, name, retry=3):
		'''
		get account usage from the backend swift
		
    		:param account: account of the user
    		:param name: name of the user
    		:param password: password of the user
		:param retry: retry how many times when the operation failed
		:returns: return a Bool object. If the user get account usage successfully from backend swift
                          then Bool.val == True. Otherwise, Bool.val == False and Bool.msg indicates the reason of failure.

		'''
		logger = util.getLogger(name="get_account_usage")
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list)==0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_account_usage,
                                                   account=account, name=name)

                return Bool(val, msg)

	@util.timeout(300)
	def __get_user_detail(self, proxyIp, account, user):
		logger = util.getLogger(name="__get_user_detail")

		url = "https://%s:8080/auth/" % proxyIp
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		cmd = "swauth-list -K %s -A %s %s %s" % (self.__password, url, account, user)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()

		if po.returncode != 0:
			msg = "Failed to get the details for user %s of account %s: %s" % (user, account, stderrData)
			logger.error(msg)
			val = False
			return Bool(val, msg)
		else:
			msg = stdoutData
			val = True
			return Bool(val, msg)

		'''
		cmd = "swauth-list -K %s -A %s %s %s" % (self.__password, url, account, user)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()

		if po.returncode != 0:
			msg = "Failed to get the details of user %s: %s" % (user, stderrData)
			logger.error(msg)
			val = False
			return Bool(val, msg)
		else:
			msg = stdoutData
			val = True
			return Bool(val, msg)
		'''

	def get_user_password(self, account, user, retry=3):
		'''
		Return password of user.

		@type  account: string
		@param account: the account name of the given user
		@type  user: string
		@param user: the user to be checked
		@type  retry: integer
		@param retry: the maximum number of times to retry after the failure
		@return: a named tuple Bool(result, val, msg). If the user
			is admin, then Bool.result == True, Bool.val == True,
			and Bool.msg == "". If the user is not admin, then
			Bool.result == False, Bool.val == True, and Bool.msg == "".
			Otherwise, Bool.result == False, Bool.val == False, and
			Bool.msg records the error message.
			
		'''
		logger = util.getLogger(name="get_user_password")

		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		user_detail = {}
		user_password = ""
		password = ""
		val = False
		msg = ""
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_user_detail, account=account, user=user)

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
			msg = "Failed to get password of user %s:%s"%(account, user)
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

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
			msg = "No proxy node is found"
			return Bool(result, val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(result, val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_user_detail, account=account, user=user)

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

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
			msg = "No proxy node is found"
			return Bool(result, val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(result, val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_user_detail, account=account, user=user)

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

	def set_account_quota(self, account, quota):
		pass

	def get_account_quota(self, account):
		pass

	def disable_account(self, account):
		pass

	def enable_account(self, account):
		pass

	@util.timeout(300)
	def __get_account_info(self, proxyIp, account):
		logger = util.getLogger(name="__get_account_info")

		url = "https://%s:8080/auth/" % proxyIp
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		cmd = "swauth-list -K %s -A %s" % (self.__password, url)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()

		if po.returncode != 0:
			msg = "Failed to get account information: %s" % stderrData
			logger.error(msg)
			val = False
			return Bool(val, msg)
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

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
			msg = "No proxy node is found"
			return Bool(result, val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(result, val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_account_info, account=account)

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

	@util.timeout(300)
	def __get_user_info(self, proxyIp, account, user):
		logger = util.getLogger(name="__get_user_info")

		url = "https://%s:8080/auth/" % proxyIp
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		cmd = "swauth-list -K %s -A %s %s" % (self.__password, url, account)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()

		if po.returncode != 0:
			msg = "Failed to get the user information of account %s: %s" % (account, stderrData)
			logger.error(msg)
			val = False
			return Bool(val, msg)
		else:
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

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
			msg = "No proxy node is found"
			return Bool(result, val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(result, val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_user_info, account=account, user=user)

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


if __name__ == '__main__':
	SA = SwiftAccountMgr()
	#print SA.add_user("test", "tester28", "testpass", True, True).msg
	#print SA.delete_user("test", "tester28").msg
	#print SA.disable_user("test", "tester28").msg
	#print SA.enable_user("test", "tester28").msg
	print SA.get_account_usage("system", "root").msg
