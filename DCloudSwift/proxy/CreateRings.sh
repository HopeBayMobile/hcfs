#!/bin/bash
# The shell script is used to install Swift's packages. (OS: Ubuntu 11.04)
# History:
# 2012/04/10 first release by Ken

if [ $# != 1 ]; then
        echo "Please enter the correct parameters!"
	echo "For example:"
	echo "If you want to use 3 replicas, type the following command."
	echo "./CreateRing.sh 3"
        exit 1
fi
Replica=$1

mkdir -p /etc/swift

cd /etc/swift
swift-ring-builder account.builder create 18 $Replica 1
swift-ring-builder container.builder create 18 $Replica 1
swift-ring-builder object.builder create 18 $Replica 1

chown -R swift:swift /etc/swift
