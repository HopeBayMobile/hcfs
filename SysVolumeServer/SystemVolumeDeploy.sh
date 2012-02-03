#!/bin/bash
# After installing Ubuntu 11.04 in /dev/sda, 
# the script is used to do the following tasks:
# (1) Install GlusterFS; (2) mount /dev/sdb
# on /export2; (3) create a system volume.
# This system volume is a GlusterFS volume of
# replica 2.
# History:
# 2012/02/03 CW First release

sudo -s << EOF
deltacloud
EOF
sudo dpkg -i ./glusterfs_3_2_3_deb/glusterfs-*.deb
sudo /etc/init.d/glusterfs-server restart
sudo mkdir -p /export1
sudo mkdir -p /export2
sudo umount /dev/sdb
sudo umount /dev/sdb1
sudo umount /dev/sdb2
sudo mkfs -t ext4 /dev/sdb << EOF
y
EOF
sudo mount /dev/sdb /export2 
IP=""
IP=`ifconfig eth0 | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}'`
sudo gluster volume create SystemVolume replica 2 $IP:/export1 $IP:/export2
sudo gluster volume start SystemVolume
echo "The mount point is $IP:/SystemVolume"
