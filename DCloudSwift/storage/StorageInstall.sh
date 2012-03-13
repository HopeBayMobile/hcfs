#!/bin/bash
# This script is used to deploy the storage node of Swift.
# History:
# 2012/02/24 first release by CW
# 2012/03/01 modified by CW 
# 2012/03/13 modified by Ken

if [ $# != 2 ]; then
	echo "Please enter the IP of proxy node and the device name as parameters!"
	echo "Example:"
	echo "./storageInstall.py 192.168.1.1 sdb1"
	exit 1
fi

ProxyIP=$1
DeviceName=$2

#Absolute path
dpkg -i /DCloudSwift/storage/deb_source/*.deb

mkdir -p /etc/swift
chown -R swift:swift /etc/swift/

echo "    StrictHostKeyChecking no" >> /etc/ssh/ssh_config
sshpass -p deltacloud scp root@$ProxyIP:/etc/swift/swift.conf /etc/swift/

IP=""
IP=`ifconfig eth0 | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}'`
export STORAGE_LOCAL_NET_IP=$IP

sshpass -p deltacloud scp root@$ProxyIP:/etc/swift/*.ring.gz /etc/swift/

umount /srv/node/sdb1
mkfs -t ext4 /dev/sdb << EOF
y
EOF
fdisk /dev/sdb << EOF
n
p
1


w
EOF
mkfs.xfs -i size=1024 /dev/sdb1 << EOF
y
EOF
echo "/dev/sdb1 /srv/node/sdb1 xfs noatime,nodiratime,nobarrier,logbufs=8 0 0" >> /etc/fstab
mkdir -p /srv/node/$DeviceName
mount /dev/sdb1 /srv/node/$DeviceName
chown -R swift:swift /srv/node

/storage/rsync.sh
perl -pi -e 's/RSYNC_ENABLE=false/RSYNC_ENABLE=true/' /etc/default/rsync
service rsync start

/storage/accountserver.sh
/storage/containerserver.sh
/storage/objectserver.sh

swift-init all start
