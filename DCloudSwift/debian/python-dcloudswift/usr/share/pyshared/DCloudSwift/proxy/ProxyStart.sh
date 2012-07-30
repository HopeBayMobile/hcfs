#!/bin/bash
# The shell script is used to start the service of Swift's proxy. (OS: Ubuntu 11.04)
# History:
# 2012/03/01 first release by CW
# 2012/03/22 modified by CW: modify MAX_META_VALUE_LENGTH from 256 to 512


chown -R swift:swift /etc/swift

#perl -pi -e 's/MAX_META_VALUE_LENGTH = 256/MAX_META_VALUE_LENGTH = 512/' /usr/share/pyshared/swift/common/constraints.py

swift-init proxy restart
