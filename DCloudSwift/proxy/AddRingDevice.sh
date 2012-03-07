#!/bin/bash
# The shell script is used to add the storage device of ring. (OS: Ubuntu 11.04)
# History:
# 2012/03/01 first release by CW

if [ $# != 3 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "./AddRingDevice.sh ZoneNumber StorageIP DeviceName"
        exit 1
fi

cd /etc/swift

ZoneNumber=$1
StorageIP=$2
DeviceName=$3

export ZONE=$ZoneNumber
export STORAGE_LOCAL_NET_IP=$StorageIP
export WEIGHT=100
export DEVICE=$DeviceName
swift-ring-builder account.builder add z$ZONE-$STORAGE_LOCAL_NET_IP:6002/$DEVICE $WEIGHT
swift-ring-builder container.builder add z$ZONE-$STORAGE_LOCAL_NET_IP:6001/$DEVICE $WEIGHT
swift-ring-builder object.builder add z$ZONE-$STORAGE_LOCAL_NET_IP:6000/$DEVICE $WEIGHT
