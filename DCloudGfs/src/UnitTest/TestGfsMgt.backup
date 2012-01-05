import nose
import sys
import os

#Packages to be tested
sys.path.append('../')
from GlusterfsMgt.GlusterfsMgt import GlusterfsMgt
#import GlusterfsMgt.GlusterfsMgt


'''
Unit Test for GlusterfsMgt.py
'''
class Test_setupSshConn:
	def setup(self):
		print "Start of Unit Test for function setupSshConn\n "
		self.gMgt = GlusterfsMgt("123")

	def teardown(self):
		print "End of Unit Test for function setupSshConn\n"
		#os.system("rm -rf ./report ./log")

	def test_SshConn(self):
		'''
		__setupSshConn(): Check the success of SSH connection
		'''
		dir(self.gMgt)
		self.gMgt._GlusterfsMgt__username = "root"
		self.gMgt._GlusterfsMgt__password = "deltacloud"
		p = self.gMgt._GlusterfsMgt__setupSshConn("127.0.0.1")
		nose.tools.ok_(p != None, "SSH failed!")

