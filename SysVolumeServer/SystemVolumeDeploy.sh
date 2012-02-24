#!/bin/bash
# After installing Ubuntu 11.04 in /dev/sda and configuring the network, 
# the script is used to do the following tasks:
# (1) Install GlusterFS and nfs-common; (2) mount /dev/sdb on /export2; (3) create a system volume;
# (4) modify /dev/fstab to mount /dev/sdb automatically;
# (5) modify /etc/rc.local to start GlusterFS service automatically every rebooting;
# (6) add all IPs of n-i hosts into /etc/hosts.
# This system volume is a GlusterFS volume of replica 2. The nfs export mounting entry is 
# xxx.xxx.xxx.xxx/SystemVolume, and use the nfs access protocol of GlusterFS.
# History:
# 2012/02/03 CW First release
# 2012/02/06 Modified by CW
# 2012/02/15 Modified by CW
# 2012/02/17 Modified by CW
# 2012/02/21 Modified by CW

echo "[`date`]: Install GlusterFS and nfs-common"
sudo dpkg -i ./deb_source/glusterfs/*.deb
sudo dpkg -i ./deb_source/nfs-common/*.deb

echo "[`date`]: Create the GlusterFS volume"
sudo /etc/init.d/glusterfs-server restart
sudo mkdir -p /export1
sudo mkdir -p /export2
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

echo "[`date`]: Modify /etc/fstab and /etc/rc.local"
sudo echo "/dev/sdb /export2 ext4 defaults 1 2" >> /etc/fstab
sudo cp ./rc.local /etc

echo "[`date`]: Add all IPs of n-i hosts into /etc/hosts"
for i in $(seq 1 255)
do
	sudo echo "192.168.11.$i" >> /etc/hosts
done

echo "[`date`]: SystemVolume is ready for use!"
echo "The mount point is $IP:/SystemVolume"
echo "The nfs mounting command is: mount -t nfs -o vers=3,nolock $IP:/SystemVolume /mnt."
