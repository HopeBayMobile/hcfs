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

#Self defined packages
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/"%BASEDIR)

from util import util
from util import threadpool
from util.SwiftCfg import SwiftCfg
from util.util import GlobalVar

UNNECESSARYFILES="cert* *.conf backups"
lock = threading.Lock()

class UnReachableHostError(Exception):
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

		self.__SC = SwiftCfg(GlobalVar.SWIFTCONF)
		self.__kwparams = self.__SC.getKwparams()
		self.__password = self.__kwparams['password']

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	@util.timeout(300)
	def __add_user(self, proxyIp, account, user, password, admin=True, reseller=False):
		logger = util.getLogger(name="__add_user")

		url = "https://%s:8080/auth/"%proxyIp
		admin_opt = "-a" if admin==True else ""
		reseller_opt = "-r" if reseller==True else ""

		cmd = "swauth-add-user -K %s -A %s %s %s %s %s %s"%(self.__password, url, admin_opt, reseller_opt, account, user, password)
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

	def add_user(self, account, user, password, admin=False, reseller=False, retry=1):
		logger = util.getLogger(name="add_user")
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None:
			msg = "Proxy list is None"
			return Bool(val, msg)

		if len(proxy_ip_list) == 0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		for t in range(retry):
			ip = random.choice(proxy_ip_list)
			try:
				output = self.__add_user(ip, account, user, password, admin, reseller)
				if output.val == True:
					val = True
					msg = output.msg
					break
				else:
					errMsg = "Failed to add user %s:%s thru %s for %s\n"%(account, user, ip, output.msg)
					logger.error(errMsg)
					msg = msg + errMsg
					
			except util.TimeoutError:
				errMsg = "Failed to add account %s thru %s in time"%(account, ip)
				logger.error("errMsg")
				msg = msg + '\n' +errMsg

                return Bool(val, msg)

	def change_password(self, account, user, newpassword):
		logger = util.getLogger(name="change_password")
		try:
			pass
		except:
			raise

	@util.timeout(300)
	def __delete_user(self, proxyIp, account, user):
		logger = util.getLogger(name="__add_account")

		url = "https://%s:8080/auth/"%proxyIp
		cmd = "swauth-delete-user -K %s -A %s %s"%(self.__password, url, account, user)
		print cmd
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

	def delete_user(self, account, user, retry=3):
		logger = util.getLogger(name="delete_user")
		proxy_ip_list = util.getProxyNodeIpList(self.__swiftDir)
		
		msg = ""
		val = False
		Bool = collections.namedtuple("Bool", "val msg")

		if proxy_ip_list is None:
			msg = "Proxy list is None"
			return Bool(val, msg)

		if len(proxy_ip_list) == 0:
			msg = "No proxy node is found"
			return Bool(val, msg)

		if retry < 1:
			msg = "Argument retry has to >= 1"
			return Bool(val, msg)

		for t in range(retry):
			ip = random.choice(proxy_ip_list)
			try:
				output = self.__delete_user(ip, account, user)
				if output.val == True:
					val = True
					msg = output.msg
					break
				else:
					errMsg = "Failed to delete user %s:%s thru %s for %s\n"%(account, user, ip, output.msg)
					logger.error(errMsg)
					msg = msg + errMsg
					
			except util.TimeoutError:
				errMsg = "Failed to delete user %s:%s thru %s in time"%(account, user, ip)
				logger.error("errMsg")
				msg = msg + '\n' +errMsg

                return Bool(val, msg)

	def list_account(self):
		logger = util.getLogger(name="list_user")
		try:
			pass
		except:
			raise

	def list_user(self, account):
		logger = util.getLogger(name="list_user")
		try:
			pass
		except:
			raise

if __name__ == '__main__':
	SA = SwiftAccountMgr()
	print SA.add_user("HELLO", "Ken", "testpass").msg
	print SA.delete_user("HELLO", "Ken").msg
	#t.start()
	#progress = SD.getCleanProgress()
	#while progress['finished'] != True:
	#	time.sleep(10)
	#	print progress
	#	progress = SD.getCleanProgress()

	
	#spreadProgress = SD.getSpreadProgress()
	#util.spreadPackages(password="deltacloud", nodeList=["172.16.229.122", "172.16.229.34", "172.16.229.46", "172.16.229.73"])
	#util.spreadRC(password="deltacloud", nodeList=["172.16.229.122"])
	#SD = SwiftDeploy([{"ip":"172.16.229.82"}], [{"ip":"172.16.229.145", "zid":1}])
	#SD = SwiftDeploy([{"ip":"172.16.229.35"}], [{"ip":"172.16.229.146", "zid":1}, {"ip":"172.16.229.35", "zid":2}])
	
