# Created on 2012/05/15 by CW
# Unit test for diskUtil.py

import nose
import sys
import os
import json
import string
import subprocess

# Import packages to be tested
sys.path.append('../src/DCloudSwift/util')
import diskUtil


class Test_getRootDisk:
	'''
	Test the function getRootDisk() in diskUtil.py
	'''
	def setup(self):
		print "Start of unit test for function getRootDisk() in diskUtil.py\n"
		self.__disk = ""
		cmd = "df -P / | grep /"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		result = po.stdout.readlines()
		po.wait()
		self.__disk = result[0].split()[0][0:8]

	def teardown(self):
		print "End of unit test for function getRootDisk() in diskUtil.py\n"

	def test_Correctness(self):
		'''
		Check the correctness of the root disk returned by getRootDisk()
		For PDCM 0.3, the root disk does not exist. As a result, we have to
		check the existence of the root disk.
		'''
		if self.__disk == None or self.__disk == "":
			pass
		else:
			test_disk = diskUtil.getRootDisk()
			nose.tools.eq_(test_disk, self.__disk, "The output of getRootDisk() is not correct!")


class Test_getAllDisks:
	'''
	Test the function getAllDisks() in diskUtil.py
	'''
	# TODO: Must check whether getAllDisks() finds out all disks.
	def setup(self):
		print "Start of unit test for function getAllDisks() in diskUtil.py\n"
		self.__result = []
		self.__result = diskUtil.getAllDisks()

	def teardown(self):
		print "End of unit test for function getAllDisks() in diskUtil.py\n"

	def test_DisksExistence(self):
		'''
		Check the existence of the disks returned by getAllDisks()
		'''
		for item in self.__result:
			nose.tools.ok_((item.startswith("/dev/sd") or item.startswith("/dev/hd")), "Device %s is not a disk!" % item)
			nose.tools.ok_(os.path.exists(item), "Disk %s does not exist!" % item)

	def test_IsRepeated(self):
		'''
		Check whether the disks returned by getAllDisks() are repeated.
		'''
		unique_result = list(set(self.__result))
		nose.tools.eq_(len(self.__result), len(unique_result), "The disks returned by getAllDisks() are repeated!")


class Test_getMountedDisks:
	'''
	Test the function getMountedDisks() in diskUtil.py
	'''
	def setup(self):
		print "Start of unit test for function getMountedDisks() in diskUtil.py\n"
		self.__result = []
		self.__result = diskUtil.getMountedDisks()

	def teardown(self):
		print "End of unit test for function getMountedDisks() in diskUtil.py\n"

	def test_IsMounted(self):
		'''
		Check whether the disks returned by getMountedDisks() are mounted.
		'''
		for item in self.__result:
			grep_result = []

			cmd = "df -P | grep %s" % item
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			grep_result = po.stdout.readlines()
			po.wait()

			nose.tools.ok_((grep_result != None and grep_result != []), "Disk %s is not mounted!" % item)

	def test_ContentIntegrity(self):
		'''
		Check whether all mounted disks are returned by getMountedDisks().
		'''
		po_result = []
		grep_result = []

		cmd = "df -P"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr = subprocess.PIPE)
		po_result = po.stdout.readlines()
		po.wait()

		for line in po_result:
			if line.split()[0][0:8].startswith("/dev/sd") or line.split()[0][0:8].startswith("/dev/hd"):
				grep_result.append(line.split()[0][0:8])

		grep_result = set(grep_result)
		unique_result = set(self.__result)
		nose.tools.eq_(grep_result, unique_result, "All mounted disks are not returned by getMountedDisks()!")

	def test_IsRepeated(self):
		'''
		Check whether the disks returned by getMountedDisks() are repeated.
		'''
		unique_result = list(set(self.__result))
		nose.tools.eq_(len(unique_result), len(self.__result), "The disks returned by getMountedDisks() are repeated!")


class Test_getUmountedDisks:
	'''
	Test the function getUmountedDisks() in diskUtil.py
	'''
	# TODO: Must check whether getUmountedDisks() finds out all unmounted disks.
	def setup(self):
		print "Start of unit test for function getUmountedDisks() in diskUtil.py\n"
		self.__result = []
		self.__result = diskUtil.getUmountedDisks()

	def teardown(self):
		print "End of unit test for function getUmountedDisks() in diskUtil.py\n"

	def test_IsUnmounted(self):
		'''
		Check whether the disks returned by getUmountedDisks() are unmounted.
		'''
		for item in self.__result:
			nose.tools.ok_(not os.path.ismount(item), "Disk %s is unmounted!" % item)

	def test_ContentIntegrity(self):
		'''
		Check whether all unmounted disks are returned by getUmountedDisks()
		'''
		pass

	def test_IsRepeated(self):
		'''
		Check whether the disks returned by getUmountedDisks() are repeated.
		'''
		unique_result = list(set(self.__result))
		nose.tools.eq_(len(unique_result), len(self.__result), "The disks returned by getUmountedDisks() are repeated!")


if __name__ == "__main__":
	#test1 = Test_getRootDisk()
	#test1.setup()
	#test2 = Test_getAllDisks()
	#test2.test_DisksExistence()
	#test2.test_IsRepeated()
	test3 = Test_getMountedDisks()
	test3.setup()
	test3.test_ContentIntegrity()
	pass
