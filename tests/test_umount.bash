#!/bin/bash
set -e
echo -e "\n======== ${BASH_SOURCE[0]} ========"
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "########## Setup Test Env"
. $WORKSPACE/utils/trace_error.bash
$WORKSPACE/utils/setup_dev_env.sh -m functional_test
. $WORKSPACE/utils/env_config.sh

if [[ $(id -Gn) != *fuse* ]]; then
	echo "########## Reload script with fuse group permission"
	exec sg fuse "${BASH_SOURCE[0]}"
fi
id
groups

echo "########## Compile binary files"
$WORKSPACE/tests/functional_test/TestCases/HCFS/compile_hcfs_bin.bash
. $WORKSPACE/tests/functional_test/TestCases/HCFS/path_config.sh

echo "########## Test unmount"
$HCFSvol unmount autotest
! mount | grep "hcfs on $WORKSPACE/tmp/mount type fuse.hcfs"

echo "########## Test delete volume"
$HCFSvol delete autotest
! $HCFSvol list | grep autotest

echo "########## Test terminate hcfs"
result=`$HCFSvol terminate`
grep -q Success <<< "$result"

echo "########## Test mount success"
