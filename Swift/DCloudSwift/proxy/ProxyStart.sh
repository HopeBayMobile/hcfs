#!/bin/bash
# The shell script is used to start the service of Swift's proxy. (OS: Ubuntu 11.04)
# History:
# 2012/03/01 first release by CW

cd /etc/swift

swift-ring-builder account.builder
swift-ring-builder container.builder
swift-ring-builder object.builder

swift-ring-builder account.builder rebalance
swift-ring-builder container.builder rebalance
swift-ring-builder object.builder rebalance

chown -R swift:swift /etc/swift

swift-init proxy start
