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

echo "########## Setup Test Env"
$repo/utils/setup_dev_env.sh -m functional_test
. $repo/utils/env_config.sh

if [[ $(id -Gn) != *fuse* ]]; then
	echo "########## Reload script with fuse group permission"
	exec sg fuse "${BASH_SOURCE[0]}"
fi
id
groups

cd $repo/tests/functional_test/TestCases/HCFS

echo "########## Start local swift"
./setup_local_swift_env.bash

echo "########## Start hcfs deamon"
./start_hcfs.bash
. path_config.sh
sleep 1

echo "########## Create hcfs volume"
	set -x
while : ; do
	result=`$HCFSvol create autotest internal 2>&1` ||:
	echo -e "[$result]"
	if grep -q error <<< "$result"; then
		sleep 1
	elif grep -q Success <<< "$result"; then
		break
	fi
done

echo "########## Test Mount"
while : ; do
	result=`$HCFSvol mount autotest $repo/tmp/mount 2>&1`
	echo -e "[$result]"
	(grep -q error <<< "$result") || break
	sleep 1
done
mount | grep "hcfs on $repo/tmp/mount type fuse.hcfs"

echo "########## Test mount success"
