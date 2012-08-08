#!/bin/bash
# The shell script is used to rebalance rings. (OS: Ubuntu 11.04)
# History:

if [ $# != 1 ]; then
        echo "Please enter the correct parameters!"
	echo "For example:"
	echo "If your swift config is put in /etc/swift, type the following command."
	echo "./Rebalance.sh /etc/swift"
        exit 1
fi

cd $1

swift-ring-builder account.builder set_min_part_hours 0
swift-ring-builder container.builder set_min_part_hours 0
swift-ring-builder object.builder set_min_part_hours 0

swift-ring-builder account.builder rebalance
swift-ring-builder container.builder rebalance
swift-ring-builder object.builder rebalance

swift-ring-builder account.builder set_min_part_hours 1
swift-ring-builder container.builder set_min_part_hours 1
swift-ring-builder object.builder set_min_part_hours 1

chown -R swift:swift $1
