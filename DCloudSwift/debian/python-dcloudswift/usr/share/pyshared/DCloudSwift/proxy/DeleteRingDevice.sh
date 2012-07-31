#!/bin/bash

if [ $# != 3 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "sh DeleteRingDevice.sh StorageIP DeviceName SwiftDir"
        exit 1
fi

cd $3

StorageIP=$1
DeviceName=$2

export STORAGE_LOCAL_NET_IP=$StorageIP
export DEVICE=$DeviceName
swift-ring-builder account.builder remove $STORAGE_LOCAL_NET_IP/$DEVICE
swift-ring-builder container.builder remove $STORAGE_LOCAL_NET_IP/$DEVICE 
swift-ring-builder object.builder remove $STORAGE_LOCAL_NET_IP/$DEVICE
