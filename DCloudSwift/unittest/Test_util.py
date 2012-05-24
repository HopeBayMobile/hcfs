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


if __name__ == "__main__":
	#ds = DaemonStatus("rsync")
	#print ds.isAlive()
	#print ds.daemonPid()
	ip = Test_getIpAddress()
	ip.setup()
	ip.test_Correctness()
