#!/bin/bash
# The shell script is used to install Swift's packages. (OS: Ubuntu 11.04)

if [ $# != 2 ]; then
        echo "Please enter the correct parameters!"
	echo "For example:"
	echo "If you want to use 3 replicas and your swift config is put in /etc/swift, type the following command."
	echo "./CreateRing.sh 3 /etc/swift"
        exit 1
fi
Replica=$1

mkdir -p $2

cd $2
swift-ring-builder account.builder create 18 $Replica 1
swift-ring-builder container.builder create 18 $Replica 1
swift-ring-builder object.builder create 18 $Replica 1

chown -R swift:swift $2
