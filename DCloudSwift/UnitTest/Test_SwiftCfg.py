# Created on 2012/04/13 by CW
# Unit test for SwiftCfg.py

import nose
import sys
import os
import json
import random
import string

# Import packages to be tested
sys.path.append('../')
from util.SwiftCfg import SwiftCfg


class CreateCfg:
	'''
	Create the configuration for testing
	'''
	def __init__(self):
		self.cfgContent = {
			"[main]": {"username": "", "password": ""},
			"[proxy]": {"replica": 4, "proxyInterface": ""},
			"[storage]": {"deviceCnt": 6, "devicePrx": "", "storageInterface": ""},
			"[log]": {"level": "", "dir": "", "name": ""},
			"[report]": {"dir": ""},
			"[timeout]": {"proxyInterval": 300, "storageInterval": 600},
			"[swiftDir]": {"dir": ""}
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

	def __assignCfgContent(self):
		for section, field in self.cfgContent.items():
			for fieldName, item in field.items():
				if self.cfgContent[section][fieldName] == "":
					self.cfgContent[section][fieldName] = self.__randStrGenerator()

		self.cfgKwparams = {
			"logDir": self.cfgContent['[log]']['dir'],
			"logLevel": self.cfgContent['[log]']['level'],
			"logName": self.cfgContent['[log]']['name'],
			"reportDir": self.cfgContent['[report]']['dir'],
			"username": self.cfgContent['[main]']['username'],
			"password": self.cfgContent['[main]']['password'],
			"proxyInterval": self.cfgContent['[timeout]']['proxyInterval'],
			"storageInterval": self.cfgContent['[timeout]']['storageInterval'],
			"numOfReplica": self.cfgContent['[proxy]']['replica'],
			"proxyInterface": self.cfgContent['[proxy]']['proxyInterface'],
			"devicePrx": self.cfgContent['[storage]']['devicePrx'],
			"deviceCnt": self.cfgContent['[storage]']['deviceCnt'],
			"storageInterface": self.cfgContent['[storage]']['storageInterface']
		}

	def __randStrGenerator(self, size=8, chars=string.letters + string.digits):
		return ''.join(random.choice(chars) for x in range(size))
		

class Test_getKwparams:
	'''
	Test the function getKwparams() in SwiftCfg.py
	'''
	def setup(self):
		print "Start of Unit Test for function getKwparams in SwiftCfg.py\n"
		self.CC = CreateCfg()
		self.CC.createCfgFile()
		self.SwCfg = SwiftCfg("./Cfg.ini")

	def teardown(self):
		print "End of Unit Test for function getKwparams in SwiftCfg.py\n"
		self.CC.removeCfgFile()

	def test_ContentIntegrity(self):
		'''
		Check the integrity of the configuration file parsed by getKwparams()
		'''
		msg = "test getkwparams"
		result = self.SwCfg.getKwparams()
		for fieldName, item in self.CC.cfgKwparams.items():
				nose.tools.eq_(result[fieldName], self.CC.cfgKwparams[fieldName], "getKwparams Error!")


if __name__ == "__main__":
	CC = CreateCfg()
	CC.createCfgFile()
	print CC.cfgContent
	print CC.cfgKwparams
	CC.removeCfgFile()
