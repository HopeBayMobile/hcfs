#!/bin/bash
#########################################################################
#
# Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract: init submodules and handles ssh key resisting
#
# Revision History
#   2016/4/25 Jethro init submodule for third_party/jansson
#
##########################################################################

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d utils ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

# init submodules
if [ ! -n "$(grep "^gitlab.hopebaytech.com " $HOME/.ssh/known_hosts)" ]; then
	ssh-keyscan gitlab.hopebaytech.com >> $HOME/.ssh/known_hosts 2>/dev/null;
fi
git submodule update --init
