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
cd $here

$repo/utils/setup_dev_env.sh -m docker_host

# Prepare Docker build resources
sudo git clean -dXf $repo

if [ -f internal.tgz ]; then
	rm -rf internal_last
	mkdir -p internal_last && pushd internal_last
	tar zxvf ../internal.tgz
	popd
fi

rsync -ra -t --delete $repo/utils/ internal/utils/
cp -f $repo/tests/functional_test/requirements.txt internal/utils/

if ! diff -aru internal/ internal_last/; then
	rm -f internal.tgz
fi

if [ ! -f internal.tgz ]; then
	( cd internal/; tar zcvf ../internal.tgz * )
fi


docker build -t docker:5000/hcfs-buildbox .
docker push docker:5000/hcfs-buildbox
