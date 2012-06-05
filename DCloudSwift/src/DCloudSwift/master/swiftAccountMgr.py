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

UNNECESSARYFILES="cert* *.conf backups"
lock = threading.Lock()
ACCOUNT_DATABASE_PATH = GlobalVar.DELTADIR+'/'+GlobalVar.ACCOUNT_DB_NAME

class UnReachableHostError(Exception):
	pass

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

		self.__accountDb = AccountDatabaseBroker(ACCOUNT_DATABASE_PATH)
		if not os.path.exists(ACCOUNT_DATABASE_PATH):
			self.__accountDb.initialize()

		self.__SC = SwiftCfg(GlobalVar.SWIFTCONF)
		self.__kwparams = self.__SC.getKwparams()
		self.__password = self.__kwparams['password']

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")


	@util.timeout(300)
	def __add_user(self, proxyIp, account, name, password, admin=True, reseller=False):
		logger = util.getLogger(name="__add_user")

		url = "https://%s:8080/auth/"%proxyIp
		admin_opt = "-a" if admin==True else ""
		reseller_opt = "-r" if reseller==True else ""

		cmd = "swauth-add-user -K %s -A %s %s %s %s %s %s"%(self.__password, url, admin_opt, reseller_opt, account, name, password)
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

	def add_user(self, account, name, password, admin=True, reseller=False, retry=1):
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
			row = self.__accountDb.add_user(account=account, name=name,
						  password=password, admin=admin,
                                                  reseller=reseller)

			if row is None:
				msg = "User %s:%s alread exists"%(account, name)
				return Bool(val, msg)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			msg = str(e)
			return Bool(val, msg)

		#TODO: Make the following code segment a function
		for t in range(retry):
			ip = random.choice(proxy_ip_list)
			try:
				output = self.__add_user(ip, account, name, password, admin, reseller)
				if output.val == True:
					val = True
					msg = output.msg
					break
				else:
					errMsg = "Failed to add user %s:%s thru %s for %s"%(account, name, ip, output.msg)
					msg = msg + '\n' + errMsg
					
			except util.TimeoutError:
				errMsg = "Failed to add account %s thru %s in time"%(account, ip)
				logger.error(errMsg)
				msg = msg + '\n' +errMsg

		try:
			if val == False:
				self.__accountDb.delete_user(account=account, name=name)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to clean user %s:%s from database for %s"%(account, name, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	def change_password(self, account, user, oldPassword, newPassword):
		logger = util.getLogger(name="change_password")
		try:
			pass
		except:
			raise

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

		for t in range(retry):
			ip = random.choice(proxy_ip_list)
			try:
				output = self.__delete_user(ip, account, name)
				if output.val == True:
					val = True
					msg = output.msg
					break
				else:
					errMsg = "Failed to delete user %s:%s thru %s for %s"%(account, name, ip, output.msg)
					msg = msg + '\n' + errMsg
					
			except util.TimeoutError:
				errMsg = "Failed to delete user %s:%s thru %s in time"%(account, name, ip)
				logger.error("errMsg")
				msg = msg + '\n' +errMsg

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
		logger = util.getLogger(name="disable_user")
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

		for t in range(retry):
			ip = random.choice(proxy_ip_list)
			try:
				output = self.__disable_user(ip, account, name)
				if output.val == True:
					val = True
					msg = output.msg
					break
				else:
					errMsg = "Failed to disable user %s:%s thru %s for %s"%(account, name, ip, output.msg)
					msg = msg + '\n' + errMsg
					
			except util.TimeoutError:
				errMsg = "Failed to disable user %s:%s thru %s in time"%(account, name, ip)
				logger.error("errMsg")
				msg = msg + '\n' +errMsg

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
		
		#TODO: make sure all the option is consistent with database and __add_user
		admin_opt = "-a" if self.__accountDb.is_admin(account, name) else ""
		reseller_opt = "-r" if self.__accountDb.is_reseller(account, name)  else ""

		cmd = "swauth-add-user -K %s -A %s %s %s %s %s %s"%(self.__password, url, admin_opt, reseller_opt, account, name, password)
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

		for t in range(retry):
			ip = random.choice(proxy_ip_list)
			try:
				output = self.__enable_user(ip, account, name)
				if output.val == True:
					val = True
					msg = output.msg
					break
				else:
					errMsg = "Failed to enable user %s:%s thru %s for %s"%(account, name, ip, output.msg)
					msg = msg + '\n' + errMsg
					
			except util.TimeoutError:
				errMsg = "Failed to enable user %s:%s thru %s in time"%(account, name, ip)
				logger.error("errMsg")
				msg = msg + '\n' +errMsg

		try:
			if val == True:
				self.__accountDb.enable_user(account=account, name=name)

		except (DatabaseConnectionError, sqlite3.DatabaseError) as e:
			errMsg = "Failed to set enabled=True for user %s:%s in database for %s"%(account, name, str(e))
			logger.error(errMsg)
			raise InconsistentDatabaseError(errMsg)

                return Bool(val, msg)

	def list_user(self, account):
		logger = util.getLogger(name="list_user")
		try:
			pass
		except:
			raise

if __name__ == '__main__':
	SA = SwiftAccountMgr()
	#print SA.add_user("test", "tester", "testpass", True, True).msg
	#print SA.delete_user("HELLO", "Ken").msg
	#print SA.disable_user("test", "tester").msg
	print SA.enable_user("test", "tester").msg
	
