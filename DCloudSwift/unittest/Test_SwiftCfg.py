# Created on 2012/04/13 by CW
# Unit test for SwiftCfg.py

import nose
import sys
import os
import json
import random
import string

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from util.SwiftCfg import SwiftCfg


class CreateCfg:
	'''
	Create the configuration file for testing
	'''
	def __init__(self):
		self.cfgContent = {
			"[storage]": {"password": "", "deviceCnt": 6},
			"[log]": {"level": "", "dir": "", "name": ""},
		}
		self.cfgKwparams = {}

	def createCfgFile(self):
		fd = open("./Cfg.ini", "w")
		self.__assignCfgContent()

		for section, field in self.cfgContent.items():
			fd.write("%s\n" % section)
			for fieldName, item in field.items():
				fd.write("%s = %s\n" % (fieldName, item))
			fd.write("\n")

	def removeCfgFile(self):
		os.system("rm ./Cfg.ini")

	def removeLogFile(self):
		os.system("rm -rf ./%s" % self.cfgKwparams["logDir"])

	def __assignCfgContent(self):
		for section, field in self.cfgContent.items():
			for fieldName, item in field.items():
				if self.cfgContent[section][fieldName] == "":
					self.cfgContent[section][fieldName] = self.__randStrGenerator()

		self.cfgKwparams = {
			"logDir": self.cfgContent['[log]']['dir'],
			"logLevel": self.cfgContent['[log]']['level'],
			"logName": self.cfgContent['[log]']['name'],
			"password": self.cfgContent['[storage]']['password'],
			"deviceCnt": self.cfgContent['[storage]']['deviceCnt']
		}

	def __randStrGenerator(self, size=8, chars=string.letters + string.digits):
		return ''.join(random.choice(chars) for x in range(size))
		

class Test_getKwparams:
	'''
	Test the function getKwparams() in SwiftCfg.py
	'''
	def setup(self):
		print "Start of unit test for function getKwparams in SwiftCfg.py\n"
		self.CC = CreateCfg()
		self.CC.createCfgFile()
		self.SwCfg = SwiftCfg("./Cfg.ini")

	def teardown(self):
		print "End of unit test for function getKwparams in SwiftCfg.py\n"
		self.CC.removeCfgFile()
		self.CC.removeLogFile()

	def test_ContentIntegrity(self):
		'''
		Check the integrity of the configuration file parsed by getKwparams()
		'''
		result = self.SwCfg.getKwparams()

		for fieldName, item in self.CC.cfgKwparams.items():
				nose.tools.eq_(result[fieldName], self.CC.cfgKwparams[fieldName], "getKwparams Error!")

	def test_ContentExistence(self):
		'''
		Check the existence of the necessary contents in the configuration file parsed by getKwparams()
		'''
		SC = SwiftCfg("../Swift.ini")
		result = SC.getKwparams()

		nose.tools.ok_(result['password'] != None, "Field \'password\' does not exist in section [storage]!")
		nose.tools.ok_(result['deviceCnt'] != None, "Field \'deviceCnt\' does not exist in section [storage]!")
		nose.tools.ok_(result['logDir'] != None, "Field \'dir\' does not exist in section [log]!")
		nose.tools.ok_(result['logLevel'] != None, "Field \'level\' does not exist in section [log]!")
		nose.tools.ok_(result['logName'] != None, "Field \'name\' does not exist in section [log]!")
		#os.system("rm %s/%s" % (result['logDir'], result['logName']))



if __name__ == "__main__":
	CC = CreateCfg()
	CC.createCfgFile()
	print CC.cfgContent
	print CC.cfgKwparams
	CC.removeCfgFile()
