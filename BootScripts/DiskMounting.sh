#!/bin/bash
# The script is placed in rc.local directory and
# executes to mount /dev/sda1 /dev/sda2 and /dev/sdb
# History:
# 2012/02/03 CW First release
# 2012/02/15 Modified by CW for checking the existence of /dev/sda and /dev/sdb

sudo /etc/init.d/glusterfs-server stop

sudo test -e /dev/sda
sdaTestCode=$?
sudo test -e /dev/sdb
sdbTestCode=$?
if [ "$sdaTestCode" != 0 ]; then
	echo "Device sda does not exist!"
	exit 1
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

sudo /etc/init.d/glusterfs-server restart
