#!/bin/bash
# The script is to partition and format two disks
# /dev/sda and /dev/sdb. It partitions /dev/sda
# into /dev/sda1(1GB) and /dev/sda2. For /dev/sda1,
# it is used for placing the configurations of GlusterFS.
# For /dev/sda2 and /dev/sdb, they are used for the
# creation of GlusterFS volume.
# Note: The script will be invoked only when creating 
# the GlusterFS volume.
# History:
# 2012/02/03 CW First release

sudo /etc/init.d/glusterfs-server stop

sudo umount /dev/sda
sudo umount /dev/sda1
sudo umount /dev/sda2
sudo umount /dev/sdb
sudo umount /dev/sdb1
sudo umount /dev/sdb2

# partition /dev/sda into /dev/sda1 and /dev/sda2
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

sudo mkdir -p /disk1
sudo mkdir -p /disk2
sudo mkdir -p /etc/glusterd

sudo mkfs -t ext4 /dev/sda1 << EOF
y
EOF
sudo mkfs -t ext4 /dev/sda2 << EOF
y
EOF
sudo mkfs -t ext4 /dev/sdb << EOF
y
EOF

sudo mount /dev/sda1 /etc/glusterd
sudo mount /dev/sda2 /disk1
sudo mount /dev/sdb /disk2
