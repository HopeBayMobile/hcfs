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

# Import packages to be tested
sys.path.append('../src/DCloudSwift/util')
import util


class DaemonStatus:
	def __init__(self, daemon_name):
		pass

	def IsAlive(self):
		pass


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
		self.__started = False

		if os.path.exists("/etc/init.d/rsync"):
			self.__installed = True

		cmd = "service rsync status"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		for item in output[0].split():
			if item.startswith("start/running"):
				self.__started = True

	def teardown(self):
		print "End of unit test for function restartRsync() in util.py\n"
		if self.__started == False:
			cmd = "service rsync stop"
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

	def test_RestartOperation(self):
		'''
		Check the restart operation of restartRsync(). 
		'''
		pass


if __name__ == "__main__":
	pass
