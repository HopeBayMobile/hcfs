# Created on 2012/05/15 by CW
# Unit test for diskUtil.py

import nose
import sys
import os
import json
import string
import time
import subprocess

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from util import diskUtil


class Test_getRootDisk:
	'''
	Test the function getRootDisk() in diskUtil.py
	'''
	def setup(self):
		print "Start of unit test for function getRootDisk() in diskUtil.py\n"
		self.__disk = ""
		self.__result = []

		cmd = "df -P / | grep /"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		result = po.stdout.readlines()
		po.wait()

		self.__disk = result[0].split()[0][0:8]
		self.__result = diskUtil.getRootDisk()

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
			nose.tools.eq_(self.__result, self.__disk, "The output of getRootDisk() is not correct!")

	def IsRepeated(self):
		'''
		Check whether the root disk returned by getRootDisk() is repeated.
		'''
		nose.tools.ok_(len(self.__result) == 1, "Function getRootDisk() returned multiple root disks!")


class Test_getAllDisks:
	'''
	Test the function getAllDisks() in diskUtil.py
	'''
	# TODO: How to check whether getAllDisks() finds out all disks?
	def setup(self):
		print "Start of unit test for function getAllDisks() in diskUtil.py\n"
		self.__result = []
		self.__result = diskUtil.getAllDisks()

	def teardown(self):
		print "End of unit test for function getAllDisks() in diskUtil.py\n"

	def test_DisksExistence(self):
		'''
		Check the existence of the disks returned by getAllDisks().
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


class Test_getNonRootDisks:
	'''
	Test the function getNonRootDisks() in diskUtil.py
	'''
	def setup(self):
		print "Start of unit test for function getNonRootDisks() in diskUtil.py\n"
		self.__disk = ""
		self.__result = []

		cmd = "df -P / | grep /"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		result = po.stdout.readlines()
		po.wait()

		self.__disk = result[0].split()[0][0:8]
		self.__result = diskUtil.getNonRootDisks()

	def teardown(self):
		print "End of unit test for function getNonRootDisks() in diskUtil.py\n"

	def test_IsNonRootDisks(self):
		'''
		Check whether the disks returned by getNonRootDisks() are non-root disks.
		'''
		for disk in self.__result:
			nose.tools.ok_(disk != self.__disk, "Root disk %s is inside the disks returned by getNonRootDisks()!" % self.__disk)

	def test_DiskExistence(self):
		'''
		Check the existence of the disks returned by getNonRootDisks().
		'''
		for item in self.__result:
			nose.tools.ok_((item.startswith("/dev/sd") or item.startswith("/dev/hd")), "Device %s is not a disk!" % item)
			nose.tools.ok_(os.path.exists(item), "Disk %s does not exist!" % item)

	def IsRepeated(self):
		'''
		Check whether the disks returned by getNonRootDisks() are repeated.
		'''
		unique_result = list(set(self.__result))
		nose.tools.eq_(len(self.__result), len(unique_result), "The disks returned by getNonRootDisks() are repeated!")


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

	def test_DisksExistence(self):
		'''
		Check the existence of the disks returned by getMountedDisks()
		'''
		for item in self.__result:
			nose.tools.ok_((item.startswith("/dev/sd") or item.startswith("/dev/hd")), "Device %s is not a disk!" % item)
			nose.tools.ok_(os.path.exists(item), "Disk %s does not exist!" % item)

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
	# TODO: How to check whether getUmountedDisks() finds out all unmounted disks?
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
		for disk in self.__result:
			mount_flag = 0

			cmd = "df -P"
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			for line in output:
				if line.split()[0][0:8] == disk:
					mount_flag = 1
					
			nose.tools.ok_(mount_flag == 0, "Device %s is mounted!" % line)

	def test_DisksExistence(self):
		'''
		Check the existence of the disks returned by getUmountedDisks()
		'''
		for item in self.__result:
			nose.tools.ok_((item.startswith("/dev/sd") or item.startswith("/dev/hd")), "Device %s is not a disk!" % item)
			nose.tools.ok_(os.path.exists(item), "Disk %s does not exist!" % item)

	def test_IsRepeated(self):
		'''
		Check whether the disks returned by getUmountedDisks() are repeated.
		'''
		unique_result = list(set(self.__result))
		nose.tools.eq_(len(unique_result), len(self.__result), "The disks returned by getUmountedDisks() are repeated!")


class Test_getMountedSwiftDevices:
	'''
	Test the function getMountedSwiftDevices() in diskUtil.py.
	'''
	def setup(self):
		print "Start of unit test for function getMountedSwiftDevices() in diskUtil.py\n"
		self.__mount_dir = "test_mount1"
		self.__prefix = "test_mount"
		self.__disk = "./test_disk"
		self.__loop_device = ""

		cmd1 = "mkdir -p /srv/node/%s" % self.__mount_dir
		po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to create Swift folders!")

		cmd2 = "dd if=/dev/zero of=%s bs=1M count=10" % self.__disk
		po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to create the disk image!")

		cmd3 = "losetup -f"
		po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to find the loop device!")

		self.__loop_device = output[0].split()[0]

		cmd4 = "losetup %s %s" % (self.__loop_device, self.__disk)
		po = subprocess.Popen(cmd4, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to launch a loop device!")

		cmd5 = "mkfs -F -t ext4 %s" % self.__loop_device
                po = subprocess.Popen(cmd5, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to make file system for the disk image!")

		cmd6 = "mount %s /srv/node/%s" % (self.__loop_device, self.__mount_dir)
		po = subprocess.Popen(cmd6, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to mount the disk image!")

	def teardown(self):
		print "End of unit test for function getMountedSwiftDevices() in diskUtil.py\n"
		cmd1 = "umount /srv/node/%s" % self.__mount_dir
		po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to umount the disk image!")

		cmd2= "losetup -d %s" % self.__loop_device
		po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to detach the loop device")

		cmd3 = "rm %s; rm -rf /srv" % self.__disk
		po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to remove the disk image and Swift folders!")

	def test_Correctness(self):
		'''
		Check whether the result returned by getMountedSwiftDevices() is correct.
		'''
		result = diskUtil.getMountedSwiftDevices(self.__prefix)
		nose.tools.ok_(len(result) == 1, "The number of mounted Swift devices returned by getMountedSwiftDevices() is not correct!")
		nose.tools.ok_(result[1] == self.__loop_device, "The devices returned by getMountedSwiftDevices() are not correct!")


class Test_getUmountedSwiftDevices:
	'''
	Test the function getUmountedSwiftDevices() in diskUtil.py.
	'''
	def setup(self):
		print "Start of unit test for function getUmountedSwiftDevices() in diskUtil.py\n"
		self.__prefix = "test_dir"
		cmd = "mkdir -p /srv/node/%s1; mkdir -p /srv/node/%s2" % (self.__prefix, self.__prefix)
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to create Swift folders!")

	def teardown(self):
		print "End of unit test for function getUmountedSwiftDevices() in diskUtil.py\n"
		cmd = "rm -rf /srv"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to remove Swift folders!")

	def test_Correctness(self):
		'''
		Check whether the result returned by getUmountedSwiftDevices() is correct.
		'''
		result = diskUtil.getUmountedSwiftDevices(2, self.__prefix)


class Test_formatDisks:
	'''
	Test the function formatDisks() in diskUtil.py
	'''
	def setup(self):
		print "Start of unit test for function formatDisks() in diskUtil.py\n"
		self.__diskImage1 = "./tmp_disk1"
		self.__diskImage2 = "./tmp_disk2"

		cmd1 = "dd if=/dev/zero of=%s bs=1M count=10" % self.__diskImage1
		po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
		nose.tools.ok_(os.path.exists(self.__diskImage1), "Disk image %s can not be created!" % self.__diskImage1)

		cmd2 = "dd if=/dev/zero of=%s bs=1M count=20" % self.__diskImage2
		po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
		nose.tools.ok_(os.path.exists(self.__diskImage2), "Disk image %s can not be created!" % self.__diskImage2)

		self.__diskList = []
		self.__diskList.append(self.__diskImage1)
		self.__diskList.append(self.__diskImage2)
		(returncode, formattedDisks) = diskUtil.formatDisks(self.__diskList)

		nose.tools.ok_(returncode == 0, "Function formatDisks() failed to format the disk images!")
		nose.tools.ok_(len(formattedDisks) == len(self.__diskList), "There exist some disks unformatted!")

	def teardown(self):
		print "End of unit test for function formatDisks() in diskUtil.py\n"
		for diskImage in self.__diskList:
			cmd = "rm %s" % diskImage
			os.system(cmd)
			nose.tools.ok_(not os.path.exists(diskImage), "Disk image %s can not be removed" % diskImage)

	def test_Ext4fsExistence(self):
		'''
		Check whether there exist ext4 file systems in disk images after invoking formatDisks().
		'''
		for diskImage in self.__diskList:
			ext4_flag = 0
			ext3_flag = 0
			cmd = "file -s %s" % diskImage
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
			output = po.stdout.readlines()
			po.wait()

			for line in output:
				for item in line.split():
					if item == "ext4":
						ext4_flag = 1
					elif item == "ext3":
						ext3_flag = 1

			nose.tools.ok_(ext3_flag == 0, "The file system of disk image %s is ext3!" % diskImage)
			nose.tools.ok_(ext4_flag == 1, "There does not exist ext4 file system in the disk image %s!" % diskImage)


class Test_mountDisk:
	'''
	Test the function mountDisk() in diskUtil.py
	'''
	def setup(self):
		print "Start of unit test for function mountDisk() in diskUtil.py\n"
		self.__diskImage = "./tmp_disk"
		self.__loopDevice = ""
		self.__mountDir = "./tmp_mnt"
		output = []

		cmd1 = "mkdir -p %s" % self.__mountDir
		os.system(cmd1)
		nose.tools.ok_(os.path.exists(self.__mountDir), "Failed to create the mount directory!")

                cmd2 = "dd if=/dev/zero of=%s bs=1M count=10" % self.__diskImage
                po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
                nose.tools.ok_(os.path.exists(self.__diskImage), "Disk image %s can not be created!" % self.__diskImage)

		cmd3 = "losetup -f"
		po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
		nose.tools.ok_(po.returncode == 0, "Failed to get the first loop device!")
		self.__loopDevice = output[0].split()[0]

		cmd4 = "losetup %s %s" % (self.__loopDevice, self.__diskImage)
		po = subprocess.Popen(cmd4, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
		nose.tools.ok_(po.returncode == 0, "Failed to attach the disk image to the loop device %s!" % self.__loopDevice)

		cmd5 = "mkfs.ext4 -F %s" % self.__loopDevice
		po = subprocess.Popen(cmd5, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
		nose.tools.ok_(po.returncode == 0, "Failed to make ext4 file system for the loop device %s!" % self.__loopDevice)

		returncode = diskUtil.mountDisk(self.__loopDevice, self.__mountDir)
                nose.tools.ok_(returncode == 0, "Function mountDisk() failed to mount the loop device %s!" % self.__loopDevice)

	def teardown(self):
		print "End of unit test for function mountDisk() in diskUtil.py\n"
		time.sleep(2)
		cmd1 = "umount %s" % self.__loopDevice
		po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
		nose.tools.ok_(po.returncode == 0, "Failed to unmount the loop device %s!" % self.__loopDevice)

		cmd2 = "losetup -d %s" % self.__loopDevice
		po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
		nose.tools.ok_(po.returncode == 0, "Failed to detach the disk image from the loop device %s!" % self.__loopDevice)

		cmd3 = "rm %s; rm -rf %s" % (self.__diskImage, self.__mountDir)
		os.system(cmd3)
		nose.tools.ok_(not os.path.exists(self.__diskImage), "Failed to remove the disk image %s!" % self.__diskImage)
		nose.tools.ok_(not os.path.exists(self.__mountDir), "Failed to remove the mount directory %s!" % self.__mountDir)

	def test_IsMounted(self):
		'''
		Check whether the disk image are mounted after invoking mountDisk().
		'''
		nose.tools.ok_(os.path.ismount(self.__mountDir), "The loop device is not mounted after invoking mountDisk()!")


class Test_mountSwiftDevice:
	'''
	Test the function mountSwiftDevice() in diskUtil.py.
	'''
	def setup(self):
		print "Start of unit test for function mountSwiftDevice() in diskUtil.py\n"
		self.__prefix = "test_dir"
		self.__deviceNum = 1
		self.__disk = "./test.img"
		self.__loop_device = ""

                cmd1 = "mkdir -p /srv/node/%s%d" % (self.__prefix, self.__deviceNum)
                po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to create Swift folders!")

		cmd2 = "dd if=/dev/zero of=%s bs=1M count=10" % self.__disk
                po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to create the disk image!")

                cmd3 = "losetup -f"
                po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to find the loop device!")

                self.__loop_device = output[0].split()[0]

                cmd4 = "losetup %s %s" % (self.__loop_device, self.__disk)
                po = subprocess.Popen(cmd4, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to launch a loop device!")

                cmd5 = "mkfs -F -t ext4 %s" % self.__loop_device
                po = subprocess.Popen(cmd5, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to make file system for the disk image!")

	def teardown(self):
		print "End of unit test for function mountSwiftDevice() in diskUtil.py\n"
		if os.path.ismount("/srv/node/%s%d" % (self.__prefix, self.__deviceNum)):
			cmd1 = "umount /srv/node/%s%d" % (self.__prefix, self.__deviceNum)
                	po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                	output = po.stdout.readlines()
                	po.wait()

                	nose.tools.ok_(po.returncode == 0, "Failed to umount the disk image!")

                cmd2= "losetup -d %s" % self.__loop_device
                po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to detach the loop device")

                cmd3 = "rm %s; rm -rf /srv" % self.__disk
                po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to remove the disk image and Swift folders!")

	def test_MountOperation(self):
		'''
		Check whether the mount operation is done by mountSwiftDevice().
		'''
		result = diskUtil.mountSwiftDevice(self.__loop_device, self.__prefix, self.__deviceNum)
		nose.tools.ok_(result == 0, "Failed to execute the mount operation performed mountSwiftDevice()!")
		nose.tools.ok_(os.path.ismount("/srv/node/%s%d" % (self.__prefix, self.__deviceNum)), "Failed to mount Swift device by mountSwiftDevice()!")


class Test_readFingerprint:
	'''
	Test the function readFingerprint() in diskUtil.py.
	'''
	def setup(self):
		print "Start of unit test for function readFingerprint() in diskUtil.py\n"
                self.__disk = "./test.img"
                self.__loop_device = ""

                cmd2 = "dd if=/dev/zero of=%s bs=1M count=10" % self.__disk
                po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to create the disk image!")

                cmd3 = "losetup -f"
                po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to find the loop device!")

                self.__loop_device = output[0].split()[0]

                cmd4 = "losetup %s %s" % (self.__loop_device, self.__disk)
                po = subprocess.Popen(cmd4, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to launch a loop device!")

		cmd5 = "mkfs -F -t ext4 %s" % self.__loop_device
                po = subprocess.Popen(cmd5, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to make file system for the disk image!")

	def teardown(self):
		print "End of unit test for function readFingerprint() in diskUtil.py\n"
                cmd2= "losetup -d %s" % self.__loop_device
                po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to detach the loop device")

                cmd3 = "rm %s" % self.__disk
                po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output = po.stdout.readlines()
                po.wait()

                nose.tools.ok_(po.returncode == 0, "Failed to remove the disk image and Swift folders!")

	def test_RaiseIOError(self):
		'''
		Check whether IOError is raised by readFingerprint() when fingerprint does not exist.
		'''
		result = diskUtil.readFingerprint(self.__loop_device)
		nose.tools.raises(IOError)

	def test_ReadOperatoin(self):
		'''
		Check the read operation performed by readFingerprint() when fingerprint exists.
		'''
		mount_dir = "./tmp_dir"
		cmd1 = "mkdir -p %s; mount %s %s; cp ./test_config/fingerprint %s" % (mount_dir, self.__loop_device, mount_dir, mount_dir)
		po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to copy the test fingerprint!")

		cmd2 = "umount %s" % mount_dir
		po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to unmount the loop device!")

		cmd3 = "rm -rf %s" % mount_dir
		po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to remove %s" % mount_dir)

		result = diskUtil.readFingerprint(self.__loop_device)
		nose.tools.ok_(result[0] == 0, "The read operation performed by readFingerprint() failed!")
		nose.tools.ok_(result[1] != None, "The content returned by readFingerprint() is not correct!")


class Test_lazyUmount:
	'''
	Test the function lazyUmount() in diskUtil.py
	'''
	def setup(self):
		print "Start of unit test for function lazyUmount() in diskUtil.py\n"
		self.__diskImage = "./tmp_disk"
		self.__mountDir = "./tmp_mnt"

                cmd1 = "dd if=/dev/zero of=%s bs=1M count=10" % self.__diskImage
                po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
                nose.tools.ok_(os.path.exists(self.__diskImage), "Disk image %s can not be created!" % self.__diskImage)

		cmd2 = "mkdir -p %s" % self.__mountDir
		os.system(cmd2)
		nose.tools.ok_(os.path.exists(self.__mountDir), "Failed to create the mount directory!")

		cmd3 = "mkfs.ext4 -F %s" % self.__diskImage
		po = subprocess.Popen(cmd3, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
		nose.tools.ok_(po.returncode == 0, "Failed to make ext4 file system for the disk image %s!" % self.__diskImage)

		cmd4 = "mount -o loop %s %s" % (self.__diskImage, self.__mountDir)
		os.system(cmd4)
		nose.tools.ok_(os.path.ismount(self.__mountDir), "Failed to mount the disk image %s!" % self.__mountDir)

		time.sleep(2)
		returncode = diskUtil.lazyUmount(self.__mountDir)
		nose.tools.ok_(returncode == 0, "Function lazyUmount() failed to unmount the disk image %s!" % self.__diskImage)

	def teardown(self):
		print "End of unit test for function lazyUmount() in diskUtil.py\n"
		time.sleep(2)
		cmd1 = "umount %s" % self.__mountDir
		po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()
		nose.tools.ok_(not os.path.ismount(self.__mountDir), "Failed to unmount the disk image %s!" % self.__diskImage)

		cmd2 = "rm %s; rm -rf %s" % (self.__diskImage, self.__mountDir)
		os.system(cmd2)
		nose.tools.ok_(not os.path.exists(self.__diskImage), "Failed to remove the disk image %s!" % self.__diskImage)
		nose.tools.ok_(not os.path.exists(self.__mountDir), "Failed to remove the mount directory %s!" % self.__mountDir)

	def test_IsUnmounted(self):
		'''
		Check whether the disk image is unmounted after invoking lazyUmount().
		'''
		nose.tools.ok_(not os.path.ismount(self.__mountDir), "The disk image is still mounted after invoking lazyUmount()!")


class Test_dumpScripts:
	'''
	Test the function dumpScripts() in diskUtil.py.
	'''
	def setup(self):
		print "Start of unit test for function dumpScripts() in diskUtil.py\n"
		self.__dump_dir = "./dump_dir"
		cmd = "mkdir -p %s" % self.__dump_dir
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to create the folder for dump!")

	def teardown(self):
		print "End of unit test for function dumpScripts() in diskUtil.py\n"
		cmd = "rm -rf %s" % self.__dump_dir
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to remove the folder for dump!")

	def test_DumpOperation(self):
		'''
		Check whether the dump operation is successfully done by dumpScripts().
		'''
		result = diskUtil.dumpScripts(self.__dump_dir)
		nose.tools.ok_(result == 0, "The execution of dumpScripts() failed!")
		nose.tools.ok_(os.listdir(self.__dump_dir) != [], "The dump operation performed by dumpScripts() failed!")


class Test_dumpSwiftMetadata:
	'''
	Test the function dumpSwiftMetadata() in diskUtil.py.
	'''
	def setup(self):
		print "Start of unit test for function dumpSwiftMetadata() in diskUtil.py\n"
		self.__backup_dir = "/backup_dir"
		self.__test_metadata = "test_metadata"
		self.__dump_dir = "./dump_dir"

		cmd1 = "mkdir -p %s; cp -r /etc/swift/* %s" % (self.__backup_dir, self.__backup_dir)
		po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to backup original Swift configuration files!")

		cmd2 = "rm -rf /etc/swift/*; touch /etc/swift/%s; mkdir -p %s" % (self.__test_metadata, self.__dump_dir)
		po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to prepare the metadata for test!")

	def teardown(self):
		print "End of unit test for function dumpSwiftMetadata() in diskUtil.py\n"
		cmd1 = "rm -rf /etc/swift/*; cp -r %s /etc/swift" % self.__backup_dir
		po = subprocess.Popen(cmd1, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to recover original Swift configuration files!")

		cmd2 = "rm -rf %s; rm -rf %s" % (self.__backup_dir, self.__dump_dir)
		po = subprocess.Popen(cmd2, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		output = po.stdout.readlines()
		po.wait()

		nose.tools.ok_(po.returncode == 0, "Failed to remove the backup of original Swift configuration files!")

	def test_DumpOperation(self):
		'''
		Check whether the dump operation is successfully done by dumpSwiftMetadata().
		'''
		result = diskUtil.dumpSwiftMetadata(self.__dump_dir)
		nose.tools.ok_(result == 0, "The execution of dumpSwiftMetadata() failed!")
		test_file_path = self.__dump_dir + "/" + self.__test_metadata
		nose.tools.ok_(os.path.exists(test_file_path), "The dump operation performed by dumpSwiftMetadata() failed!")



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
