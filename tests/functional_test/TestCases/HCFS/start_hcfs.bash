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

exec 1> >(while read line; do echo -e "        $line"; done;)
exec 2> >(while read line; do echo -e "        $line" >&2; done;)

echo "########## Setup Test Env"
$repo/utils/setup_dev_env.sh -m functional_test
. $repo/utils/env_config.sh

if [[ $(id -Gn) != *fuse* ]]; then
	echo "########## Reload script with fuse group permission"
	exec sg fuse "${BASH_SOURCE[0]}"
fi
id

# Main cource code
function cleanup {
	echo "########## Cleanup"
	sudo umount $repo/tmp/mount* |& grep -v "not mounted" || :
	sudo pgrep -a "hcfs " || :
	sudo killall -9 'hcfs' || :
	mkdir -p $repo/tmp/mount
	sudo find $repo/tmp/mount* -mindepth 1 -delete
}

echo "########## Compile binary files"
$here/compile_hcfs_bin.bash
. $here/path_config.sh
cd $repo/src/API
make hcfsconf
rm -f /data/hcfs.conf
./hcfsconf enc /data/hcfs.conf.plain /data/hcfs.conf

echo "########## Start hcfs deamon"
cleanup
set -x
cd $repo/tmp
$hcfs -d -oallow_other |& cut -c -120 &
until [ -f $repo/tmp/hcfs_android_log ]
do
	sleep 1
done
set +x
