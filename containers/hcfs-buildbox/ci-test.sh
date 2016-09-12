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
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

if id -u jenkins >/dev/null 2>&1; then
	echo "jenkins ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/jenkins
	sudo -E -u jenkins run-parts --exit-on-error --verbose $here/scrips
else
	run-parts --exit-on-error --verbose $here/scrips
fi
