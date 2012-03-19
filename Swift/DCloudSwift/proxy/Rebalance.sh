#!/bin/bash
# The shell script is used to rebalance rings. (OS: Ubuntu 11.04)
# History:
# 2012/03/13 first release by Ken

cd /etc/swift

swift-ring-builder account.builder set_min_part_hours 0
swift-ring-builder container.builder set_min_part_hours 0
swift-ring-builder object.builder set_min_part_hours 0

swift-ring-builder account.builder rebalance
swift-ring-builder container.builder rebalance
swift-ring-builder object.builder rebalance

swift-ring-builder account.builder set_min_part_hours 1
swift-ring-builder container.builder set_min_part_hours 1
swift-ring-builder object.builder set_min_part_hours 1

chown -R swift:swift /etc/swift
