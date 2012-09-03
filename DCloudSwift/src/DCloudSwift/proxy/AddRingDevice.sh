#!/bin/bash
# The shell script is used to add the storage device of ring. (OS: Ubuntu 11.04)

if [ $# != 5 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "./AddRingDevice.sh ZoneNumber StorageIP DeviceName Weight swiftDir"
        exit 1
fi

cd $5

ZoneNumber=$1
StorageIP=$2
DeviceName=$3
Weight=$4

export ZONE=$ZoneNumber
export STORAGE_LOCAL_NET_IP=$StorageIP
export WEIGHT=$Weight
export DEVICE=$DeviceName
swift-ring-builder account.builder add z$ZONE-$STORAGE_LOCAL_NET_IP:6002/$DEVICE $WEIGHT
swift-ring-builder container.builder add z$ZONE-$STORAGE_LOCAL_NET_IP:6001/$DEVICE $WEIGHT
swift-ring-builder object.builder add z$ZONE-$STORAGE_LOCAL_NET_IP:6000/$DEVICE $WEIGHT
