#!/bin/bash
##
## Copyright (c) 2021 HopeBayTech.
##
## This file is part of Tera.
## See https://github.com/HopeBayMobile for further info.
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##

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
