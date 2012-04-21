#!/bin/bash
# The script is executed by /etc/rc.local. The main function 
# is to check which service has been deployed by PDCM, for example, 
# OpenStack or Swift. Then the corresponding script is invoked
# to deal with the remaining task.
# History:
# 2012/03/22 CW First release


# waiting for the completion of other booting processes
sleep 10


# check which service has been deployed by PDCM
if [ ! -e /dev/sda ]; then
	echo -e "\n[`date`]: ERROR Device sda does not exist. Please check the status of sda."
	exit 1
fi

if [ ! -e /dev/sda1 ]; then
	echo -e "\n[`date`]: INFO The host has not been deployed."
	exit 0
else
	MountCode=0
	mount /dev/sda1 /mnt
	MountCode=$?
fi

if [ "$MountCode" == 0 ]; then
	
fi

# Copy the configuration of GlusterFS in RAM
# and try to mount /dev/sda1 on /etc/glusterd
sudo umount /dev/sda1
sudo mkdir -p /etc/glusterd
sudo cp -r /etc/glusterd /glusterdTmp
sudo mount /dev/sda1 /etc/glusterd
MountCode=$?

TestCode=1
if [ "$MountCode" == 0 ]; then
	sudo test -f /etc/glusterd/FirstBootDone
	TestCode=$?
fi

if [ "$MountCode" == 0 ] && [ "$TestCode" == 0 ]; then
	sudo rm -rf /glusterdTmp
	echo "Old Configurations of GlusterFS exist!"
else
# FirstBootDone does not exist and it means that /dev/sda1 
# is a new partition.
	sudo umount /dev/sda
	sudo umount /dev/sda1
	sudo umount /dev/sda2
	sudo umount /dev/sda3
	sudo umount /dev/sda4
	sudo mkfs -t ext4 /dev/sda << EOF
y
EOF
	sudo fdisk /dev/sda << EOF
n
p
1
1
+1G
n
p
2


w
EOF
	sudo mkfs -t ext4 /dev/sda1 << EOF
y
EOF
	sudo mkfs -t ext4 /dev/sda2 << EOF
y
EOF
	sudo mount /dev/sda1 /etc/glusterd
	sudo cp -r /glusterdTmp/* /etc/glusterd
	sudo touch /etc/glusterd/FirstBootDone
	# FirstBootDone is a mark for the old configurations of GlusterFS
	sudo rm -rf /glusterdTmp
	sudo mkfs -t ext4 /dev/sdb << EOF
y
EOF
fi

# Mount /dev/sda2 and /dev/sdb for the usage of GlusterFS
sudo mkdir -p /GlusterHD
if [ "$sdaTestCode" == 0 ] && [ "$sdbTestCode" != 0 ]; then
	sudo mount /dev/sda2 /GlusterHD
	sudo mkdir -p /GlusterHD/disk1
	sudo mkdir -p /GlusterHD/disk2
	sudo /etc/init.d/glusterfs-server restart
	exit 1
fi
sudo mkdir -p /GlusterHD/disk1
sudo mkdir -p /GlusterHD/disk2
sudo mount /dev/sda2 /GlusterHD/disk1
sudo mount /dev/sdb /GlusterHD/disk2

