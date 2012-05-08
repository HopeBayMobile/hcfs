#!/bin/bash
# The shell script is used to remove a device from the ring. (OS: Ubuntu 11.04)
# History:
# 2012/03/15 first release by Ken

if [ $# != 2 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "./RmRingDevice.sh StorageIP DeviceName"
        exit 1
fi

cd /etc/swift

StorageIP=$1
DeviceName=$2

export STORAGE_LOCAL_NET_IP=$StorageIP
export DEVICE=$DeviceName
swift-ring-builder account.builder remove $STORAGE_LOCAL_NET_IP/$DEVICE
swift-ring-builder container.builder remove $STORAGE_LOCAL_NET_IP/$DEVICE 
swift-ring-builder object.builder remove $STORAGE_LOCAL_NET_IP/$DEVICE
