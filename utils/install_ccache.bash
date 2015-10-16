#!/bin/bash
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
configfile="$WORKSPACE/utils/env_config.sh"
# Use ccache to speedup compile
if ! echo $PATH | grep -E "(^|:)/usr/lib/ccache(:|$)"; then
	echo "export PATH=\"/usr/lib/ccache:$PATH\"" >> "$configfile"
fi
if ! ccache -V | grep 3.2; then
	if ! apt-cache policy ccache | grep 3.2; then
		source /etc/lsb-release
		echo "deb http://ppa.launchpad.net/comet-jc/ppa/ubuntu $DISTRIB_CODENAME main" \
			| sudo tee /etc/apt/sources.list.d/comet-jc-ppa-trusty.list
		sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 32EF5841642ADD17
		sudo apt-get update
	fi
	force_install="$force_install ccache"
fi
echo "export USE_CCACHE=1" >> "$configfile"
