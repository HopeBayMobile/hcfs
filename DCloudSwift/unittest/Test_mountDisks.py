# Created on 2012/05/15 by CW
# Unit test for mountDisks.py

import nose
import sys
import os
import json
import string
import subprocess


WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(WORKING_DIR)

# Import packages to be tested
#path = '%s/src/DCloudSwift/'%BASEDIR
#sys.path.insert(0,'%s/src/DCloudSwift/util'%BASEDIR)
#import mountDisks

#sys.path.append("../src/DCloudSwift")
#from util import mountDisks


class Test_getRootDisk:
	'''
	Test the function getRootDisk() in mountDisks.py
	'''
	def setup(self):
		print "Start of Unit Test for function getRootDisk() in mountDisks.py\n"
		self.__disk = ""
		cmd = "df -P / | grep /"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		result = po.stdout.readlines()
		po.wait()
		self.__disk = result[0].split()[0][0:8]
		print "The root disk is %s" % self.__disk

	def teardown(self):
		print "End of Unit Test for function getRootDisk() in mountDisks.py\n"

	def test_Correctness(self):
		'''
		Check the correctness of the root disk returned by getRootDisk()
		For PDCM 0.3, the root disk does not exist. As a result, we have to
		check the existence of the root disk.
		'''
		if self.__disk == None or self.__disk == "":
			pass
		else:
			test_disk = mountDisks.getRookDisk()
			nose.tools.eq_(test_disk, result_disk, "The output of getRootDisk() is not correct!")



if __name__ == "__main__":
	test = Test_getRootDisk()
	test.setup()
