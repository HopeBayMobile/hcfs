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
set -v

echo "########## Setup Test Env"
$repo/utils/setup_dev_env.sh -m functional_test
. $repo/utils/env_config.sh

if [[ $(id -Gn) != *fuse* ]]; then
	echo "########## Reload script with fuse group permission"
	exec sg fuse "${BASH_SOURCE[0]}"
fi
id
groups

echo "########## Compile binary files"
$repo/tests/functional_test/TestCases/HCFS/compile_hcfs_bin.bash
. $repo/tests/functional_test/TestCases/HCFS/path_config.sh

echo "########## Test unmount"
$HCFSvol unmount autotest
! mount | grep "hcfs on $repo/tmp/mount type fuse.hcfs"

echo "########## Test delete volume"
$HCFSvol delete autotest
! $HCFSvol list | grep autotest

echo "########## Test terminate hcfs"
result=`$HCFSvol terminate`
grep -q Success <<< "$result"

echo "########## Test mount success"
