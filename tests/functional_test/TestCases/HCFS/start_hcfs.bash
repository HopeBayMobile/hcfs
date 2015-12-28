#!/bin/bash
exec 1> >(while read line; do echo -e "        $line"; done;)
exec 2> >(while read line; do echo -e "        $line" >&2; done;)
set -e
echo -e "\n======== ${BASH_SOURCE[0]} ========"
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../../../.. && pwd )"
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

# Main cource code
function cleanup {
	echo "########## Cleanup"
	sudo umount $WORKSPACE/tmp/mount* |& grep -v "not mounted" || :
	sudo pgrep -a "hcfs " || :
	sudo killall -9 'hcfs' || :
	mkdir -p $WORKSPACE/tmp/mount
	sudo find $WORKSPACE/tmp/mount* -mindepth 1 -delete
}

echo "########## Compile binary files"
$here/compile_hcfs_bin.bash
. $here/path_config.sh

echo "########## Start hcfs deamon"
cleanup
set -x
$hcfs -d -oallow_other |& cut -c -120 &
set +x
