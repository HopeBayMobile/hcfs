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

docker_workspace=/var/jenkins/workspace/HCFS

sudo rm -rf $repo/utils/.setup_*
$repo/utils/setup_dev_env.sh -v -m docker_host

# Start test slave
if docker ps | grep hcfs_test; then
	timeout 10 sudo docker stop hcfs_test
fi
sudo docker rm -f hcfs_test || :
sudo docker pull docker:5000/docker_hcfs_test_slave

# Running auto test
sudo docker run --rm -t --privileged --name=hcfs_test \
	-v $repo:$docker_workspace \
	-v /var/run/docker.sock:/var/run/docker.sock \
	-e CCACHE_DIR=$docker_workspace/.ccache \
	docker:5000/docker_hcfs_test_slave \
	/sbin/my_init -- $docker_workspace/containers/hcfs-test-slave/ci-test.sh
