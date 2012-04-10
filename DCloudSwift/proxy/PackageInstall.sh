#!/bin/bash
# The shell script is used to install Swift's packages. (OS: Ubuntu 11.04)
# History:
# 2012/02/24 first release by CW
# 2012/03/01 modified by CW
# 2012/03/06 modified by CW: check the existence of IP address
# 2012/03/17 modified by Ken
# 2012/03/22 modified by CW: modify the absolute path of directory deb_source


if [ $# != 1 ]; then
        echo "Please enter the correct parameters!"
	echo "For example:"
	echo "If you want to use 3 replicas, type the following command."
	echo "./PackageInstall.sh 3"
        exit 1
fi
Replica=$1


dpkg -i /DCloudSwift/proxy/deb_source/*.deb


mkdir -p /etc/swift
chown -R swift:swift /etc/swift/


cat >/etc/swift/swift.conf <<EOF
[swift-hash]
# random unique string that can never change (DO NOT LOSE)
swift_hash_path_suffix = `od -t x8 -N 8 -A n </dev/random`
EOF


IP=""
IP=`ifconfig eth0 | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}'`
if [ "$IP" = "" ]; then
	echo "The IP address of the proxy node does not exist!"
	exit 1
fi
export PROXY_LOCAL_NET_IP=$IP


openssl req -new -x509 -nodes -out cert.crt -keyout cert.key << EOF
TW
Taiwan
Taipei
Delta
CTC
CW
cw.luo@delta.com.tw
EOF
mv cert.key /etc/swift
mv cert.crt /etc/swift


perl -pi -e "s/-l 127.0.0.1/-l $PROXY_LOCAL_NET_IP/" /etc/memcached.conf
service memcached restart


/DCloudSwift/proxy/ProxyConfigCreation.sh


cd /etc/swift
swift-ring-builder account.builder create 18 $Replica 1
swift-ring-builder container.builder create 18 $Replica 1
swift-ring-builder object.builder create 18 $Replica 1

