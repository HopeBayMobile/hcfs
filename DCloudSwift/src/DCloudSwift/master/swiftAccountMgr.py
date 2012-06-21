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
		 
		@type  proxy_ip_list: string  
    	@param proxy_ip_list: ip list of proxy nodes
    	@type  retry: integer
		@param retry: the maximum number of times to retry when fn return the False
    	@type  fn: string
    	@param fn: private function to call
    	@param kwargs: keyword arguments to fn
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
	def __add_user(self, proxyIp, account, user, password, admin=True, reseller=False):
		logger = util.getLogger(name="__add_user")
		self.__class__.__add_user.__name__
		
		url = "https://%s:8080/auth/"%proxyIp
		msg = ""
		val = False

		admin_opt = "-a " if admin else ""
		reseller_opt = "-r " if reseller  else ""
		optStr = admin_opt+reseller_opt

		cmd = "swauth-add-user -K %s -A %s %s %s %s %s"%(self.__password, url, optStr, account, user, password)
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

	def add_user(self, account, user, password, admin=True, reseller=False, retry=3):
		'''
		add user to the database and backend swift

		@type  account: string
		@param account: the name of the given account
		@type  user: string
		@param user: the name of the given user
		@type  password: string
		@param password: the password to be set
		@type  admin: boolean
		@param admin: admin or not
		@type  reseller: boolean
		@param reseller: reseller or not
		@type  retry: integer
		@param retry: the maximum number of times to retry when fn return the False
		@return: a tuple (val, msg). If the user is successfully added to both the database and backend swift
			then Bool.val == True and msg records the standard output. Otherwise, val == False and msg records the error message.
		
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
			row = self.__accountDb.add_user(account=account, name=user)

			if row is None:
				msg = "User %s:%s alread exists"%(account, user)
				return Bool(val, msg)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			msg = str(e)
			return Bool(val, msg)
		
		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__add_user,
                                                   account=account, user=user, password=password,
						   admin=admin, reseller=reseller)

		try:
			if val == False:
				self.__accountDb.delete_user(account=account, name=user)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to clean user %s:%s from database for %s"%(account, user, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	@util.timeout(300)
	def __delete_user(self, proxyIp, account, user):
		logger = util.getLogger(name="__delete_user")

		url = "https://%s:8080/auth/"%proxyIp
		cmd = "swauth-delete-user -K %s -A %s %s %s"%(self.__password, url, account, user)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		(stdoutData, stderrData) = po.communicate()
				
		msg = ""
		val = False

		if po.returncode !=0 and '404' not in stderrData:
			logger.error(stderrData)
			msg = stderrData
			val =False
		elif '404' in stderrData:
			msg = "user %s:%s does not exists"%(account, user)
			logger.warn(msg)
			val =True
		else:
			logger.info(stdoutData)
			msg = stdoutData
			val =True

		Bool = collections.namedtuple("Bool", "val msg")
                return Bool(val, msg)

	def delete_user(self, account, user, retry=3):
		'''
		delte user from the database and backend swift

		@type  account: string
		@param account: the name of the given account
		@type  user: string
		@param user: the name of the given user
		@type  retry: integer
		@param retry: the maximum number of times to retry when fn return the False
		@return: a tuple (val, msg). If the user is successfully deleted to both the database and backend swift
			then Bool.val == True and msg records the standard output. Otherwise, val == False and msg records the error message.
		
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
                                                   account=account, user=user)

		try:
			if val == True:
				self.__accountDb.delete_user(account=account, name=user)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to clean user %s:%s from database for %s"%(account, user, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	def add_account(self, account, retry=3):
		'''
		add account and create an default admin to the database and backend swift

		@type  account: string
		@param account: the name of the given account
		@type  password: string
		@param password: the password to be set
		@type  retry: integer
		@param retry: the maximum number of times to retry when fn return the False
		@return: a tuple (val, msg). 
		
		'''
		
#		logger = util.getLogger(name="add_account")
#		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
#		
#		msg = ""
#		val = False
#		Bool = collections.namedtuple("Bool", "val msg")
#
#		if proxy_ip_list is None or len(proxy_ip_list) ==0:
#			msg = "No proxy node is found"
#			return Bool(val, msg)
#
#		if retry < 1:
#			msg = "Argument retry has to >= 1"
#			return Bool(val, msg)
#
#		try:
#			row = self.__accountDb.add_user(account=account, name=user)
#
#			if row is None:
#				msg = "User %s:%s alread exists"%(account, user)
#				return Bool(val, msg)
#
#		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
#			msg = str(e)
#			return Bool(val, msg)
#		
#		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__add_user,
#                                                   account=account, user=user, password=password,
#						   admin=admin, reseller=reseller)
#
#		try:
#			if val == False:
#				self.__accountDb.delete_user(account=account, name=user)
#
#		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
#			errMsg = "Failed to clean user %s:%s from database for %s"%(account, user, str(e))
#			logger.error(errMsg)
#			raise InconsistentDatabaseError(errMsg)
#
#                return Bool(val, msg)

	def delete_account(self, account):
		'''
		check user
		'''
		pass

	@util.timeout(300)
	def __enable_user(self, proxyIp, account, user):
		logger = util.getLogger(name="__enable_user")

		url = "https://%s:8080/auth/"%proxyIp
		msg = ""
		val = False

		password = self.__accountDb.get_password(account, user)
		if password is None:
			msg = "user %s:%s does not exists"%(account,user)
			return Bool(val, msg)
		
		admin_opt = "-a " if self.__accountDb.is_admin(account, user) else ""
		reseller_opt = "-r " if self.__accountDb.is_reseller(account, user)  else ""
		optStr = admin_opt + reseller_opt

		cmd = "swauth-add-user -K %s -A %s %s %s %s %s"%(self.__password, url, optStr, account, user, password)
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

	def enable_user(self, account, user, retry=3):
		'''
		Enable the user to access the backend swift by re-adding it to the backend using original setting stored in database.

		@type  account: string
		@param account: the name of the given account
		@type  user: string
		@param user: the name of the given user
		@type  retry: integer
		@param retry: the maximum number of times to retry when fn return the False
		@return: a tuple (val, msg). If the user is re-added to the backend using the original setting stored in the backend.
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
                                                   account=account, user=user)
		try:
			if val == True:
				self.__accountDb.enable_user(account=account, name=user)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to set enabled=True for user %s:%s in database for %s"%(account, user, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	@util.timeout(300)
	def __disable_user(self, proxyIp, account, user):
		logger = util.getLogger(name="__disable_user")

		url = "https://%s:8080/auth/"%proxyIp
		randomPassword = str(uuid.uuid4())
		cmd = "swauth-add-user -K %s -A %s %s %s %s"%(self.__password, url, account, user, randomPassword)
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

	def disable_user(self, account, user, retry=3):
		'''
		Disabe the user from accessing the backend swift by changing the user's password in the backend.

		@type  account: string
		@param account: the name of the given account
		@type  user: string
		@param user: the name of the given user
		@type  retry: integer
		@param retry: the maximum number of times to retry when fn return the False
		@return: a tuple (val, msg). If the user's backend password is successfully changed
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
                                                   account=account, user=user)

		try:
			if val == True:
				self.__accountDb.disable_user(account=account, name=user)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to set enabled=False for user %s:%s in database for %s"%(account, user, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	def enable_account(self, account):
		pass
	
	def disable_account(self, account):
		pass

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
		@return: a tuple (val, msg). If the operation is successfully done,
		then val == True and msg records the standard output. Otherwise,
		val == False and msg records the error message. 
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
                cmd = "swauth-add-user -K %s -A %s %s %s %s %s"%(self.__password, url, optStr, account, user, newPassword)
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                (stdoutData, stderrData) = po.communicate()

                if po.returncode !=0:
			msg = "Failed to change the password of %s: %s" % (user, stderrData)
                        logger.error(msg)
                        val = False
                else:
                        msg = stdoutData
			logger.info(msg)
                        val = True

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
		user_detail = {}
		admin = False
		reseller = False
		ori_password = ""

		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
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

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_user_detail, account=account, user=user)

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

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__change_password,\
		account=account, user=user, newPassword=newPassword, admin=admin, reseller=reseller)

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
		@return: a tuple (val, msg). If the operatoin is successfully
			done, then val == True and msg records the information
			of the given user. Otherwise, val == False and msg
			records the error message.
		'''
		
		logger = util.getLogger(name="__get_account_user")

		url = "https://%s:8080/auth/v1.0"%proxyIp
		password = self.get_user_password(account, user).msg
		cmd = "swift -A %s -U %s:%s -K %s stat"%(url, account, user, password)
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

	def get_account_usage(self, account, user, retry=3):
		'''
		get account usage from the backend swift
		
		@type  account: string 
		@param account: the account name of the given user
		@type  user: string
		@param user: the user to be checked
    		@type  retry: integer
		@param retry: the maximum number of times to retry after the failure
	    	@return: a named tuple Bool(val, msg). If get the account usage
			successfully, then Bool.val == True, and Bool.msg == "". 
			Otherwise, Bool.val == False, and Bool.msg records the error message. 

		'''
		logger = util.getLogger(name="get_account_usage")
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		
		msg = ""
		val = False
		user_detail = {}
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list)==0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry, fn=self.__get_account_usage,
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
		@return: a tuple (val, msg). If the operatoin is successfully
			done, then val == True and msg records the information
			of the given user. Otherwise, val == False and msg
			records the error message.
		'''
		logger = util.getLogger(name="__get_user_detail")

		url = "https://%s:8080/auth/" % proxyIp
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		cmd = "swauth-list -K %s -A %s %s %s" % (self.__password, url, account, user)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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
		@return: a named tuple Bool(val, msg). If get the user's password
			successfully, then Bool.val == True, and Bool.msg == password. 
			Otherwise, Bool.val == False, and Bool.msg records the error message.
			
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

	@util.timeout(300)
	def __get_account_info(self, proxyIp):
		'''
		Get the account information of Swift.

		@type  proxyIp: string
		@param proxyIp: IP of the proxy node
		@return: a tuple (val, msg). If the operation is successfully
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
		@return: a named tuple Bool(result, val, msg). If the get the account
			list successfully, then Bool.val == True, and Bool.msg == account list. 
			If the account list does not exist, then Bool.val == True, and Bool.msg == "".
			Otherwise, Bool.val == False, and Bool.msg records the error message.
		'''
		logger = util.getLogger(name="list_account")

		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		account_info = {}
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

		msg = account_info["accounts"]

		return Bool(val, msg)

	def list_user(self, account, retry=3):
                '''             
                List all the existed accounts.
                
                @type  account: string
                @param account: the account name of the given user
                @type  retry: integer
                @param retry: the maximum number of times to retry after the failure
                @return: a named tuple Bool(val, msg). If the get the user
                        list successfully, then Bool.val == True, and Bool.msg == user list. 
                        If the user list does not exist, then Bool.val == True, and Bool.msg == "".
                        Otherwise, Bool.val == False, and Bool.msg records the error message.
                '''
                logger = util.getLogger(name="list_user")

                proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
                user_info = {}
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
                fn=self.__get_user_info, account=account)

                if val == False:
                        return Bool(val, msg)

                try:
                        user_info = json.loads(msg)
                        val = True
                        msg = ""

                except Exception as e:
                        msg = "Failed to load the json string: %s" % str(e)
                        logger.error(msg)
                        val = False
                        return Bool(val, msg)

                msg = user_info["users"]

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
                @return: a named tuple Bool(val, msg). If the container list is got
                        successfully, then Bool.val == True and Bool.msg == user list. 
                        If the container list does not exist, then Bool.val == True and Bool.msg == "".
                        Otherwise, Bool.val == False, and Bool.msg records the error message.
                '''
                logger = util.getLogger(name="list_container")

                proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
                val = False
                msg = ""
		Bool = collections.namedtuple("Bool", "val msg")

                if proxy_ip_list is None or len(proxy_ip_list) ==0:
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

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
                fn=self.__get_container_info, account=account, admin_user=admin_user,\
		admin_password=admin_password)

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
		@return: a tuple (val, msg). If the operation is successfully
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
		@return: a tuple (val, msg). If the operation is successfully
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

		if po.returncode != 0:
			msg = stderrData
			logger.error(msg)
			val = False
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
		fn=self.__get_user_info, account=account)

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
	def __get_read_acl(self, proxyIp, account, container, admin_user, admin_password):
		'''
		Get the read acl of the container

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
		@return: a named tuple Bool(val, msg). If the read acl is successfully
			gotten, then val == True and msg == "". Otherwise, val ==
			False and msg records the error message.
		'''
		logger = util.getLogger(name="__get_read_acl")

                url = "https://%s:8080/auth/v1.0" % proxyIp
                msg = "Failed to get the read acl of container %s:" % container
                val = False
		Bool = collections.namedtuple("Bool", "val msg")

                cmd = "swift -A %s -U %s:%s -K %s stat %s"%(url, account, admin_user, admin_password, container)
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
				msg = line.split("ACL: ")[1]
				logger.info(msg)
				val = True

		if val == False:
			msg = msg + " " + stderrData

                return Bool(val, msg)

	@util.timeout(300)
	def __get_write_acl(self, proxyIp, account, container, admin_user, admin_password):
		'''
		Get the write acl of the container

		@type  proxyIp: string
		@param proxyIp: IP of the proxy node
		@type  account: string
		@param account: the account of the container
		@type  container: string
		@param container: the container to get the write acl
		@type  admin_user: string
		@param admin_user: the admin user of the account
		@type  admin_password: string
		@param admin_password: the password of admin_user
		@return: a named tuple Bool(val, msg). If the write acl is successfully
			gotten, then val == True and msg == "". Otherwise, val ==
			False and msg records the error message.
		'''
		logger = util.getLogger(name="__get_write_acl")

                url = "https://%s:8080/auth/v1.0" % proxyIp
                msg = "Failed to get the write acl of container %s:" % container
                val = False
		Bool = collections.namedtuple("Bool", "val msg")

                cmd = "swift -A %s -U %s:%s -K %s stat %s"%(url, account, admin_user, admin_password, container)
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                (stdoutData, stderrData) = po.communicate()

                if po.returncode != 0:
			msg = msg + " " + stderrData
                        logger.error(msg)
                        val = False
			return Bool(val, msg)

		lines = stdoutData.split("\n")

		for line in lines:
			if "Write" in line:
				msg = line.split("ACL: ")[1]
				logger.info(msg)
				val = True

		if val == False:
			msg = msg + " " + stderrData

                return Bool(val, msg)

	@util.timeout(300)
	def __set_read_acl(self, proxyIp, account, container, admin_user, admin_password, read_acl):
		'''
		Set the read acl of the container

		@type  proxyIp: string
		@param proxyIp: IP of the proxy node
		@type  account: string
		@param account: the account of the container
		@type  container: string
		@param container: the container to set the read acl
		@type  admin_user: string
		@param admin_user: the admin user of the account
		@type  admin_password: string
		@param admin_password: the password of admin_user
		@type  read_acl: string
		@param read_acl: the read acl to be set to that of the container
		@return: a named tuple Bool(val, msg). If the read acl is successfully
			set, then val == True and msg == "". Otherwise, val ==
			False and msg records the error message.
		'''
		logger = util.getLogger(name="__set_read_acl")

                url = "https://%s:8080/auth/v1.0" % proxyIp
                msg = "Failed to set the read acl of container %s:" % container
                val = False
		Bool = collections.namedtuple("Bool", "val msg")

                cmd = "swift -A %s -U %s:%s -K %s post -r \'%s\' %s"%(url, account, admin_user,\
		admin_password, read_acl, container)
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
	def __set_write_acl(self, proxyIp, account, container, admin_user, admin_password, write_acl):
		'''
		Set the write acl of the container

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
		@param read_acl: the write acl to be set to that of the container
		@return: a named tuple Bool(val, msg). If the write acl is successfully
			set, then val == True and msg == "". Otherwise, val ==
			False and msg records the error message.
		'''
		logger = util.getLogger(name="__set_write_acl")

                url = "https://%s:8080/auth/v1.0" % proxyIp
                msg = "Failed to set the write acl of container %s:" % container
                val = False
		Bool = collections.namedtuple("Bool", "val msg")

                cmd = "swift -A %s -U %s:%s -K %s post -w \'%s\' %s" % (url, account, admin_user,\
		admin_password, write_acl, container)
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
		@return: a named tuple Bool(val, msg). If the read acl is successfully
			assigned, then val == True and msg == "". Otherwise, val ==
			False and msg records the error message.
		'''
		logger = util.getLogger(name="assign_read_acl")

		#TODO: Check the existence of container
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		ori_read_acl = ""
		admin_password = ""

		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
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

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_read_acl, account=account, container=container, admin_user=admin_user,\
		admin_password=admin_password)

		if val == False:
			return Bool(val, msg)
		else:
			ori_read_acl = msg

			if "%s:%s" % (account, user) in ori_read_acl:
				val = True
				msg = ""
				return Bool(val, msg)

			ori_read_acl = ori_read_acl + "," + "%s:%s" % (account, user)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__set_read_acl, account=account, container=container, admin_user=admin_user,\
		admin_password=admin_password, read_acl=ori_read_acl)

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
		@return: a named tuple Bool(val, msg). If the write acl is successfully
			assigned, then val == True and msg == "". Otherwise, val ==
			False and msg records the error message.
		'''
		logger = util.getLogger(name="assign_write_acl")

		#TODO: Check the existence of container
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		ori_write_acl = ""
		admin_password = ""

		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
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

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_write_acl, account=account, container=container, admin_user=admin_user,\
		admin_password=admin_password)

		if val == False:
			return Bool(val, msg)
		else:
			ori_write_acl = msg

			if "%s:%s" % (account, user) in ori_write_acl:
				val = True
				msg = ""
				return Bool(val, msg)

			ori_write_acl = ori_write_acl + "," + "%s:%s" % (account, user)

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__set_write_acl, account=account, container=container, admin_user=admin_user,\
		admin_password=admin_password, write_acl=ori_write_acl)

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
		@return: a named tuple Bool(val, msg). If the read acl is successfully
			removed, then val == True and msg == "". Otherwise, val ==
			False and msg records the error message.
		'''
		logger = util.getLogger(name="remove_read_acl")

		#TODO: Check the existence of container
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		ori_read_acl = ""
		admin_password = ""

		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
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

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_read_acl, account=account, container=container, admin_user=admin_user,\
		admin_password=admin_password)

		if val == False:
			return Bool(val, msg)

		ori_read_acl = msg
		new_read_acl = ""
		ori_read_acl = ori_read_acl.split(",")
		account_user_pattern = account + ":" + user

		while account_user_pattern in ori_read_acl:
			ori_read_acl.remove(account_user_pattern)

		for item in ori_read_acl:
			new_read_acl = new_read_acl + item + ","

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__set_read_acl, account=account, container=container, admin_user=admin_user,\
		admin_password=admin_password, read_acl=new_read_acl)

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
		@return: a named tuple Bool(val, msg). If the write acl is successfully
			removed, then val == True and msg == "". Otherwise, val ==
			False and msg records the error message.
		'''
		logger = util.getLogger(name="remove_write_acl")

		#TODO: Check the existence of container
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		ori_write_acl = ""
		admin_password = ""

		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None or len(proxy_ip_list) ==0:
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

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__get_write_acl, account=account, container=container, admin_user=admin_user,\
		admin_password=admin_password)

		if val == False:
			return Bool(val, msg)

		ori_write_acl = msg
		new_write_acl = ""
		ori_write_acl = ori_write_acl.split(",")
		account_user_pattern = account + ":" + user

		while account_user_pattern in ori_write_acl:
			ori_write_acl.remove(account_user_pattern)

		for item in ori_write_acl:
			new_write_acl = new_write_acl + item + ","

		(val, msg) = self.__functionBroker(proxy_ip_list=proxy_ip_list, retry=retry,\
		fn=self.__set_write_acl, account=account, container=container, admin_user=admin_user,\
		admin_password=admin_password, write_acl=new_write_acl)

		return Bool(val, msg)


if __name__ == '__main__':
	SA = SwiftAccountMgr()
	#print SA.add_user("rice", "rice01", "rice01", True, False).msg
	print SA.add_user("rice", "rice01", "rice02", True, False).msg
	print SA.add_user("rice", "rice01", "rice02", True, False).msg
	#print SA.delete_user("test", "tester28").msg
	#print SA.disable_user("test", "tester28").msg
	#print SA.enable_user("test", "tester28").msg
	#print SA.get_account_usage("system", "root").msg
	print SA.list_account().msg
	print SA.list_user("rice").msg
