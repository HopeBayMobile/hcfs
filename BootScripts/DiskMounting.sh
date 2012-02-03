#!/bin/bash
# The script is placed in rc.local directory and
# executes to mount /dev/sda1 /dev/sda2 and /dev/sdb
# History:
# 2012/02/03 CW First release

sudo /etc/init.d/glusterfs-server stop

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
	sudo umount /dev/sda1
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
sudo mkdir -p /disk1
sudo mkdir -p /disk2
sudo mount /dev/sda2 /disk1
sudo mount /dev/sdb /disk2

sudo /etc/init.d/glusterfs-server restart
