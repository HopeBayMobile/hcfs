#!/bin/bash
echo ======== ${BASH_SOURCE[0]} ========
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../.. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "########## Setup Test Env"
$WORKSPACE/utils/setup_dev_env.sh -m functional_test
if [[ $(id -Gn) != *fuse* ]]; then
	echo "########## Reload script with fuse group permission"
	exec sg fuse "${BASH_SOURCE[0]}"
fi
id
groups
. $WORKSPACE/utils/env_config.sh
. $WORKSPACE/utils/trace_error.bash
set -e

# Setup for $hcfs and $HCFSvol
$here/Scripts/compile_hcfs_bin.bash
. $here/Scripts/path_config.sh

$here/Scripts/start_swift.bash
#$here/Scripts/test_mount.bash

function cleanup {
	echo "########## Cleanup"
	sudo umount $WORKSPACE/tmp/mount* |& grep -v "not mounted" || :
	sudo pgrep -a "hcfs " || :
	sudo pkill 'hcfs' || :
	mkdir -p $WORKSPACE/tmp/{meta,block,mount}
	sudo find $WORKSPACE/tmp/{meta,block,mount*} -mindepth 1 -delete
}
cleanup
trap cleanup EXIT

cd "$here"
python pi_tester.py -d debug -c HCFS_0
