# Created on 2012/04/21 by CW
# Unit test for util.py

import nose
import sys
import os
import json
import random
import string
import socket
import subprocess
import time
import socket

# Import packages to be tested
sys.path.append('../src/DCloudSwift/util')
import util


class DaemonStatus:
	def __init__(self, daemon_name):
		self.__daemon_name = daemon_name
		self.__pid = "NoSuchPid"

		cmd = "cat /var/run/%s.pid" % daemon_name
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		if po.returncode == 0 and output != None:
			self.__pid = output[0].split()[0]
		else:
			self.__pid = "NoSuchPid"

	def isAlive(self):
		if self.__pid == "NoSuchPid":
			return False
		else: 
			cmd = "cat /proc/%s/status" % self.__pid
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			if po.returncode != 0:
				return False
			elif output[0].split()[1] == self.__daemon_name:
				return True
			else:
				return False

	def daemonPid(self):
		return self.__pid


class SwiftDaemonStatus:
	def __init__(self):
		self.__daemonStatus = {
			'account-auditor': [False, "NoSuchPid"],
			'account-reaper': [False, "NoSuchPid"],
			'account-replicator': [False, "NoSuchPid"],
			'account-server': [False, "NoSuchPid"],
			'container-auditor': [False, "NoSuchPid"],
			'container-replicator': [False, "NoSuchPid"],
			'container-server': [False, "NoSuchPid"],
			'container-updater': [False, "NoSuchPid"],
			'object-auditor': [False, "NoSuchPid"],
			'object-replicator': [False, "NoSuchPid"],
			'object-server': [False, "NoSuchPid"],
			'object-updater': [False, "NoSuchPid"],
			'proxy-server': [False, "NoSuchPid"]
		}

	def __checkDaemonStatus(self):
		for daemon_name, status in self.__daemonStatus.items():
			if os.path.exists("/var/run/swift/%s.pid" % daemon_name):
				cmd = "cat /var/run/swift/%s.pid" % daemon_name
				po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
				output = po.stdout.readlines()
				po.wait()

				if po.returncode == 0:
					output = output[0].split()[0]
					if os.path.exists("/proc/%s" % output):
						status[0] = True
						status[1] = output
				else:
					status[0] = False

	def getSwiftHealth(self):
		black_list = 0

		for daemon_name, status in self.__daemonStatus.items():
			if status[0] == False:
				black_list += 1

		if black_list == 0:
			return True
		else:
			return False

	def getDaemonStatus(self):
		self.__checkDaemonStatus()
		return self.__daemonStatus


class Test_isDaemonAlive:
	'''
	Test the function isDaemonAlive() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function isDaemonAlive() in util.py\n"

	def teardown(self):
		print "End of unit test for function isDaemonAlive() in util.py\n"

	def test_initDaemon(self):
		'''
		Check whether the status of daemon init returned by isDaemonAlive() is true.
		'''
		daemon_name = "init"
		cmd = "ps -ef | grep init"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(util.isDaemonAlive(daemon_name), "Erroneous status of daemon %s reported by isDaemonAlive()!" % daemon_name)

	def test_rsyslogDaemon(self):
		'''
		Check the status of daemon rsyslog returned by isDaemonAlive().
		'''
		daemon_name = "rsyslog"
		status_flag = False
		cmd = "service rsyslog status"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		for item in output[0].split():
			if item.startswith("start/running"):
				status_flag = True

		nose.tools.eq_(util.isDaemonAlive(daemon_name), status_flag, "Erroneous status of daemon %s reported by isDaemonAlive()!" % daemon_name)

	def test_NoSuchDaemon(self):
		'''
		Check whether the status of NoSuchDaemon returned by isDaemonAlive() is false.
		'''
		daemon_name = "NoSuchDaemon"
		nose.tools.ok_(not util.isDaemonAlive(daemon_name), "Erroneous status of daemon %s reported by isDaemonAlive()!" % daemon_name)


class Test_isValid:
	'''
	Test the function isValid() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function isValid() in util.py\n"
		self.__vers = "vers"
		self.__test_vers = "test_vers"
		self.__test_hostname = "test_hostname"
		self.__hostname = socket.gethostname()

		self.__fingerprint = {
			"vers": self.__vers,
			"hostname": self.__hostname
		}

	def teardown(self):
		print "End of unit test for function isValid() in util.py\n"

	def test_Version(self):
		'''
		Check the correctness of the output of isValid() by versions.
		'''
		output = False

		output = util.isValid(self.__vers, self.__fingerprint)
		nose.tools.ok_(output, "The output of isValid() should be true!")

		output = util.isValid(self.__test_vers, self.__fingerprint)
		nose.tools.ok_(not output, "The output of isValid() should be false!")

	def test_Hostname(self):
		'''
		Check the correctness of the output of isValid() by hostname.
		'''
		output = False

		output = util.isValid(self.__vers, self.__fingerprint)
		nose.tools.ok_(output, "The output of isValid() should be true!")

		self.__fingerprint["hostname"] = self.__test_hostname
		output = util.isValid(self.__vers, self.__fingerprint)
		nose.tools.ok_(not output, "The output of isValid() should be false!")


class Test_restartRsync:
	'''
	Test the function restartRsync() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function restartRsync() in util.py\n"
		self.__installed = False
		self.__ds = DaemonStatus("rsync")
		self.__started = self.__ds.isAlive()
		self.__pid = self.__ds.daemonPid()

		if os.path.exists("/etc/init.d/rsync"):
			self.__installed = True
		else:
			sys.exit(0)

		if self.__started == False:
			cmd = "/etc/init.d/rsync start"
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			new_ds = DaemonStatus("rsync")
			self.__pid = new_ds.daemonPid()
			nose.tools.ok_(new_ds.isAlive(), "Error: Daemon rsync can not be started!")

	def teardown(self):
		print "End of unit test for function restartRsync() in util.py\n"
		if self.__installed == True:
			if self.__started == False:
				cmd = "/etc/init.d/rsync stop"
			else:
				cmd = "/etc/init.d/rsync start"

			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			nose.tools.ok_(po.returncode == 0, "Daemon rsync can not be started/stopped!")

	def test_RestartOperation(self):
		'''
		Check the restart operation of restartRsync(). 
		'''
		nose.tools.ok_(self.__installed, "Package rsync does not be installed!")

		result = -1
		test_alive = False
		test_pid = "NoSuchPid"

		result = util.restartRsync()
		test_ds = DaemonStatus("rsync")
		test_alive = test_ds.isAlive()
		test_pid = test_ds.daemonPid()

		nose.tools.ok_(result == 0, "The execution of restartRsync() failed!")
		nose.tools.ok_(test_alive, "Daemon rsync can not be restarted by restartRsync()!")
		nose.tools.ok_(test_pid != self.__pid, "The pid of daemon rsync is the same after invoking restartRsync()!")


class Test_startRsync:
	'''
	Test the function startRsync() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function startRsync() in util.py\n"
		self.__installed = False
		self.__ds = DaemonStatus("rsync")
		self.__started = self.__ds.isAlive()
		self.__pid = self.__ds.daemonPid()

		if os.path.exists("/etc/init.d/rsync"):
			self.__installed = True
		else:
			sys.exit(0)

		if self.__started == True:
			cmd = "/etc/init.d/rsync stop"
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			new_ds = DaemonStatus("rsync")
			self.__pid = new_ds.daemonPid()
			nose.tools.ok_(self.__pid == "NoSuchPid", "Pid %s of daemon rsyn still exists!" % self.__pid)
			nose.tools.ok_(new_ds.isAlive() == False, "Error: Daemon rsync can not be stopped!")

	def teardown(self):
		print "End of unit test for function startRsync() in util.py\n"
		if self.__installed == True:
                        if self.__started == False:
                                cmd = "/etc/init.d/rsync stop"
                        else:
                                cmd = "/etc/init.d/rsync start"

                        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                        output = po.stdout.readlines()
                        po.wait()

                        nose.tools.ok_(po.returncode == 0, "Daemon rsync can not be started/stopped!")

	def test_StartOperation(self):
		'''
		Check the start operation of startRsync().
		'''
		nose.tools.ok_(self.__installed, "Package rsync does not be installed!")

		result = -1
		test_alive = False
		test_pid = "NoSuchPid"
		time.sleep(2)

		result = util.startRsync()
		test_ds = DaemonStatus("rsync")
		test_alive = test_ds.isAlive()
		test_pid = test_ds.daemonPid()

		nose.tools.ok_(result == 0, "The execution of startRsync() failed!")
		nose.tools.ok_(test_alive, "Daemon rsync can not be started by restartRsync()!")


class Test_restartMemcached:
	'''
	Test the function restartMemcached() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function restartMemcached() in util.py\n"
		self.__installed = False
		self.__ds = DaemonStatus("memcached")
		self.__started = self.__ds.isAlive()
		self.__pid = self.__ds.daemonPid()

		if os.path.exists("/etc/init.d/memcached"):
			self.__installed = True
		else:
			sys.exit(0)

		if self.__started == False:
			cmd = "/etc/init.d/memcached start"
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			new_ds = DaemonStatus("memcached")
			self.__pid = new_ds.daemonPid()
			nose.tools.ok_(new_ds.isAlive(), "Error: Daemon memcached can not be started!")

	def teardown(self):
		print "End of unit test for function restartMemcached() in util.py\n"
		if self.__installed == True:
			if self.__started == False:
				cmd = "/etc/init.d/memcached stop"
			else:
				cmd = "/etc/init.d/memcached start"

			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			nose.tools.ok_(po.returncode == 0, "Daemon memcached can not be started/stopped!")

	def test_RestartOperation(self):
		'''
		Check the restart operation of restartMemcached().
		'''
		nose.tools.ok_(self.__installed, "Package memcached does not be installed!")

		result = -1
		test_alive = False
		test_pid = "NoSuchPid"

		result = util.restartMemcached()
		test_ds = DaemonStatus("memcached")
		test_alive = test_ds.isAlive()
		test_pid = test_ds.daemonPid()

                nose.tools.ok_(result == 0, "The execution of restartMemcached() failed!")
                nose.tools.ok_(test_alive, "Daemon memcached can not be restarted by restartMemcached()!")
                nose.tools.ok_(test_pid != self.__pid, "The pid of daemon memcached is the same after invoking restartMemcached()!")


class Test_startMemcached:
	'''
	Test the function startMemcached() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function startMemcached() in util.py\n"
		self.__installed = False
		self.__ds = DaemonStatus("memcached")
		self.__started = self.__ds.isAlive()
		self.__pid = self.__ds.daemonPid()

		if os.path.exists("/etc/init.d/memcached"):
			self.__installed = True
		else:
			sys.exit(0)

		if self.__started == True:
			cmd = "/etc/init.d/memcached stop"
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			new_ds = DaemonStatus("memcached")
			self.__pid = new_ds.daemonPid()
			nose.tools.ok_(self.__pid == "NoSuchPid", "Pid %s of daemon memcached still exists!" % self.__pid)
			nose.tools.ok_(new_ds.isAlive() == False, "Error: Daemon memcached can not be stopped!")

	def teardown(self):
                print "End of unit test for function startMemcached() in util.py\n"
		if self.__installed == True:
			if self.__started == False:
				cmd = "/etc/init.d/memcached stop"
			else:
				cmd = "/etc/init.d/memcached start"

			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			nose.tools.ok_(po.returncode == 0, "Daemon memcached can not be started/stopped!")

	def test_StartOperation(self):
		'''
		Check the start operation of startMemcached().
		'''
		nose.tools.ok_(self.__installed, "Package memcached does not be installed!")

		result = -1
		test_alive = False
		test_pid = "NoSuchPid"
		time.sleep(2)

		result = util.startMemcached()
		test_ds = DaemonStatus("memcached")
		test_alive = test_ds.isAlive()
		test_pid = test_ds.daemonPid()

		nose.tools.ok_(result == 0, "The execution of startMemcached() failed!")
		nose.tools.ok_(test_alive, "Daemon memcached can not be started by restartMemcached()!")


class Test_getDeviceCnt:
	'''
	Test the function getDeviceCnt() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function getDeviceCnt() in util.py\n"
		self.__result = util.getDeviceCnt()

	def teardown(self):
		print "End of unit test for function getDeviceCnt() in util.py\n"

	def test_OutputExistence(self):
		'''
		Check the existence of the output of getDeviceCnt().
		'''
		nose.tools.ok_(self.__result != "" and self.__result != None, "The output of getDeviceCnt() is empty!")

	def test_IsInteger(self):
		'''
		Check whether the output of getDeviceCnt() is an integer.
		'''
		nose.tools.eq_(self.__result, int(self.__result), "The device count returned by getDeviceCnt() is not an integer!")

	def test_OutputRange(self):
		'''
		Check whether the output of getDeviceCnt() is in the range (0, 100).
		'''
		nose.tools.ok_(self.__result > 0, "The device count returned by getDeviceCnt() is less than zero!")
		nose.tools.ok_(self.__result < 100, "The device count returned by getDeviceCnt() is larger than 100!")


class Test_getDevicePrx:
	'''
	Test the function getDevicePrx() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function getDevicePrx() in util.py\n"

	def teardown(self):
		print "End of unit test for function getDevicePrx() in util.py\n"

	def test_OutputExistence(self):
		'''
		Check the existence of the output of getDevicePrx().
		'''
		result = util.getDevicePrx()
		nose.tools.ok_(result != None and result != "", "The output of getDevicePrx() is empty!")


class Test_getIpAddress:
	'''
	Test the function getIpAddress() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function getIpAddress() in util.py\n"
		self.__hostname = socket.gethostname()
		self.__result = util.getIpAddress()

	def teardown(self):
		print "End of unit test for function getIpAddress() in util.py\n"

	def test_Correctness(self):
		'''
		Check whether the ip retruned by getIpAddress() is the ip of the server.
		'''
		existence_flag = False
		hostname = ""
		aliaslist = []
		ipaddrlist = []
		msg = ""

		try:
			(hostname, aliaslist, ipaddrlist) = socket.gethostbyaddr(self.__result)
		except Exception as e:
			msg = str(e)
			nose.tools.ok_(False, msg)
		finally:
			if hostname.startswith(self.__hostname) or hostname == "localhost":
				existence_flag = True

		nose.tools.ok_(existence_flag == True, "The ip returned by getIpAddress() is not the ip the server!")

	def test_NotLocalIp(self):
		'''
		Check whether the ip returned by getIpAddress() is not the local ip.
		'''
		nose.tools.ok_(not self.__hostname.startswith("127."), "The ip returned by getIpAddress() is the local ip!")


class Test_stopDaemon:
	'''
	Test the function stopDaemon() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function stopDaemon() in util.py\n"
		self.__ds = DaemonStatus("rsyslog")
		self.__started = self.__ds.isAlive()

		if self.__started == False:
			cmd = "/etc/init.d/rsyslog start"
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			nose.tools.ok_(po.returncode == 0, "Failed to start the daemon rsyslog!")

	def teardown(self):
		print "End of unit test for function stopDaemon() in util.py\n"
		cmd = "/etc/init.d/rsyslog restart"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to restart the daemon rsyslog!")

	def test_StopOperation(self):
		'''
		Check the stop operation of stopDaemon().
		'''
		output = util.stopDaemon("rsyslog")

		test_ds = DaemonStatus("rsyslog")
		test_started = test_ds.isAlive()

		nose.tools.ok_(output == 0, "The execution of stopDaemon() failed!")
		nose.tools.ok_(not test_started, "Daemon rsyslog can not be stopped by stopDaemon()!")


class Test_generateSwiftConfig:
	'''
	Test the function generateSwiftConfig() in util.py.
	'''
	def setup(self):
		print "Start of unit test for function generateSwiftConfig() in util.py\n"
		self.__rsync_config = False

		if os.path.exists("/etc/rsyncd.conf"):
			self.__rsync_config = True
			cmd = "mkdir -p /etc/tmp_backup; mv /etc/swift/* /etc/rsyncd.conf /etc/tmp_backup"
		else:
			self.__rsync_config = False
			cmd = "mkdir -p /etc/tmp_backup; mv /etc/swift/* /etc/tmp_backup"

		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to backup original Swift configuration files!")

	def teardown(self):
		print "End of unit test for function generateSwiftConfig() in util.py\n"
		if self.__rsync_config == True:
			cmd = "rm -rf /etc/swift/*; mv /etc/tmp_backup/rsyncd.conf /etc; cp -r /etc/tmp_backup/* /etc/swift; rm -rf /etc/tmp_backup"
		else:
			cmd = "rm -rf /etc/swift/*; cp -r /etc/tmp_backup/* /etc/swift; rm -rf /etc/tmp_backup"

		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to recover original Swift configuration files!")

	def test_ConfigExistence(self):
		'''
		Check the existence of Swift configuration files.
		'''
		swift_config_list = {
			'swift.conf': '/etc/swift/',
			'cert.key': '/etc/swift/',
			'cert.crt': '/etc/swift/',
			'proxy-server.conf': '/etc/swift/',
			'rsyncd.conf': '/etc/',
			'account-server.conf': '/etc/swift/',
			'container-server.conf': '/etc/swift/',
			'object-server.conf': '/etc/swift/'
		}

		util.generateSwiftConfig()

		for file_name, path in swift_config_list.items():
			nose.tools.ok_(os.path.exists(path + file_name), "Configuration file %s does not exist!" % file_name)


class Test_restartAllServices:
	'''
	Test the function restartAllServices() in util.py.
	'''
	def setup(self):
		return
		print "Start of unit test for function restartAllServices() in util.py\n"
		self.__rsync_daemon = {
			'name': 'rsync',
			'config': '/etc/rsyncd.conf',
			'config_name': 'rsyncd.conf',
			'config_dir': '/etc/',
			'path': '/etc/init.d/rsync',
			'test_config': './test_config/rsyncd.conf'
		}

		self.__memcached_daemon = {
			'name': 'memcached',
			'config': '/etc/memcached.conf',
			'config_name': 'memcached.conf',
			'config_dir': '/etc/',
			'path': '/etc/init.d/memcached',
			'test_config': './test_config/memcached.conf'
		}

		self.__swift_daemon = {
			'name': 'swift',
			'config': '/etc/swift/',
			'config_name': 'swift',
			'config_dir': '/etc/',
			'path': 'swift-init all',
			'test_config': './test_config/swift'
		}

		self.__service_list = [self.__rsync_daemon, self.__memcached_daemon, self.__swift_daemon]

		cmd1 = "mkdir -p /etc/tmp_backup"
		returncode = os.system(cmd1)
		nose.tools.ok_(returncode == 0, "Failed to make the directory to backup the configuration files!")

		for item in self.__service_list:
			cmd2 = "mv %s /etc/tmp_backup" % item['config']
			cmd3 = "cp -r %s %s" % (item['test_config'], item['config_dir'])
			cmd4 = cmd2 + "; " + cmd3
			po = subprocess.Popen(cmd4, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			nose.tools.ok_(po.returncode == 0, "Failed to backup the configuration files of %s!" % item['name'])

			cmd5 = item['path'] + " start"
			po = subprocess.Popen(cmd5, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
			output = po.stdout.readlines()
			po.wait()

			nose.tools.ok_(po.returncode == 0, "Failed to start %s daemon!" % item['name'])

		ori_rsync_status = DaemonStatus('rsync')
		self.__rsync_started = ori_rsync_status.isAlive()
		self.__rsync_pid = ori_rsync_status.daemonPid()

		ori_memcached_status = DaemonStatus('memcached')
		self.__memcached_started = ori_memcached_status.isAlive()
		self.__memcached_pid = ori_memcached_status.daemonPid()

		ori_swift_status = SwiftDaemonStatus()
		self.__swift_pid = ori_swift_status.getDaemonStatus()
		self.__swift_started = ori_swift_status.getSwiftHealth()

		util.restartAllServices()

	def teardown(self):
		return
		print "End of unit test for function restartAllServices() in util.py\n"
		for item in self.__service_list:
			cmd1 = "rm -rf %s" % item['config']
			cmd2 = "cp -r /etc/tmp_backup/%s %s" % (item['config_name'], item['config_dir'])
			cmd3 = "rm -rf /etc/tmp_backup"
			cmd = cmd1 + "; " + cmd2 + "; " + cmd3
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			nose.tools.ok_(po.returncode == 0, "Failed to recover the configuration files of %s!" % item['name'])

			cmd4 = item['path'] + " restart"
			po = subprocess.Popen(cmd4, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			nose.tools.ok_(po.returncode == 0, "Failed to restart %s daemon!" % item['name'])

	def test_RsyncStarted(self):
		'''
		Check the restart operation of daemon rsync triggered by restartAllServices().
		'''
		return
		test_rsync_status = DaemonStatus('rsync')
                test_rsync_started = test_rsync_status.isAlive()
                test_rsync_pid = test_rsync_status.daemonPid()

		nose.tools.ok_(test_rsync_started, "Daemon rsync does not be started by restartAllServices()!")
		nose.tools.ok_(test_rsync_pid != self.__rsync_pid, "Failed to restart rsync daemon by invoking restartAllServices()!")

	'''
	def test_MemcachedStarted(self):
		
		Check the restart operatoin of daemon memcached triggered by restartAllServices().
		
		pass

	def test_SwiftStarted(self):
		
		Check the restart operation of Swift daemons triggered by restartAllServices().
		
		pass
	'''


if __name__ == "__main__":
	#ds = DaemonStatus("rsync")
	#print ds.isAlive()
	#print ds.daemonPid()
	ip = Test_getIpAddress()
	ip.setup()
	ip.test_Correctness()
