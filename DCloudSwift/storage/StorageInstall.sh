#!/bin/bash
# This script is used to deploy the storage node of Swift.
# History:
# 2012/02/24 first release by CW
# 2012/03/01 modified by CW 
# 2012/03/13 modified by Ken
# 2012/03/14 modified by Ken
# 2012/03/22 modified by CW: modify MAX_META_VALUE_LENGTH from 256 to 512

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

python /DCloudSwift/storage/mountDisks.py 1

chown -R swift:swift /srv/node

/DCloudSwift/storage/rsync.sh
perl -pi -e 's/RSYNC_ENABLE=false/RSYNC_ENABLE=true/' /etc/default/rsync
service rsync start

/DCloudSwift/storage/accountserver.sh
/DCloudSwift/storage/containerserver.sh
/DCloudSwift/storage/objectserver.sh

perl -pi -e 's/MAX_META_VALUE_LENGTH = 256/MAX_META_VALUE_LENGTH = 512/' /usr/share/pyshared/swift/common/constraints.py

swift-init all start
