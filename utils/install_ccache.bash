#!/bin/bash
#########################################################################
#
# Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Revision History
#   2016/1/18 Jethro unified usage of workspace path
#
##########################################################################

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d utils ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

configfile="$repo/utils/env_config.sh"

source $repo/utils/common_header.bash

# Use ccache to speedup compile
if ! ccache -V | grep 3.2; then
	if ! apt-cache policy ccache | grep 3.2; then
		source /etc/lsb-release
		echo "deb http://ppa.launchpad.net/comet-jc/ppa/ubuntu $DISTRIB_CODENAME main" \
			| sudo tee /etc/apt/sources.list.d/comet-jc-ppa-trusty.list
		sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 32EF5841642ADD17
	fi
	force_install="$force_install ccache"
	install_pkg
fi

echo "export PATH=\"/usr/lib/ccache:$PATH\"" >> "$configfile"
echo "export USE_CCACHE=1" >> "$configfile"
awk -F'=' '{seen[$1]=$0} END{for (x in seen) print seen[x]}' "$configfile" > /tmp/awk_tmp
sudo mv -f /tmp/awk_tmp "$configfile"
sudo chmod --reference="${BASH_SOURCE[0]}" "$configfile"
