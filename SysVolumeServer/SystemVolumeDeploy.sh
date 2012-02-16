#!/bin/bash
# After installing Ubuntu 11.04 in /dev/sda and
# configuring the network, 
# the script is used to do the following tasks:
# (1) Install GlusterFS and nfs-kernel-server; (2) mount /dev/sdb
# on /export2; (3) create a system volume;
# (4) modify /dev/fstab to mount /dev/sdb automatically;
# (5) modify rc.local to execute nfs_ServiceStart.sh automatically
# every rebooting.
# This system volume is a GlusterFS volume of
# replica 2. The nfs export directory is /SystemVolume.
# History:
# 2012/02/03 CW First release
# 2012/02/06 Modified by CW
# 2012/02/15 Modified by CW

sudo dpkg -i ./deb_source/*.deb
sudo /etc/init.d/glusterfs-server restart
sudo mkdir -p /export1
sudo mkdir -p /export2
sudo mkdir -p /SystemVolume
sudo umount /dev/sdb
sudo umount /dev/sdb1
sudo umount /dev/sdb2
sudo umount /dev/sdb3
sudo umount /dev/sdb4
sudo mkfs -t ext4 /dev/sdb << EOF
y
EOF
sudo mount /dev/sdb /export2 
IP=""
IP=`ifconfig eth1 | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}'`
sudo gluster volume create SystemVolume replica 2 $IP:/export1 $IP:/export2
sudo gluster volume start SystemVolume
sudo mount -t glusterfs $IP:/SystemVolume /SystemVolume
sudo echo "/SystemVolume *(rw,no_root_squash,fsid=0)" >> /etc/exports
sleep 5
sudo /etc/init.d/nfs-kernel-server restart
sudo echo "/dev/sdb /export2 ext4 defaults 1 2" >> /etc/fstab
sudo cp /SysVolumeServer/rc.local /etc
echo "The mount point is $IP:/SystemVolume"
