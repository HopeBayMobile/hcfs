#!/bin/bash
echo ======== ${BASH_SOURCE[0]} ========
date
set -x -e

host_workspace="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
docker_workspace=/var/jenkins/workspace/HCFS

sudo rm -rf $host_workspace/utils/.setup_*
$host_workspace/utils/setup_dev_env.sh -v -m docker_host

# Start test slave
if docker ps | grep hcfs_test; then
	timeout 10 sudo docker stop hcfs_test
fi
sudo docker rm -f hcfs_test || :
sudo docker pull docker:5000/docker_hcfs_test_slave
SLAVE_ID=$(sudo docker run -d -t \
		--privileged \
		-v /tmp/ccache:/home/jenkins/.ccache \
		-v $host_workspace:$docker_workspace \
		-v /var/run/docker.sock:/var/run/docker.sock \
		--name=hcfs_test \
		docker:5000/docker_hcfs_test_slave)

# Running auto test
docker exec $SLAVE_ID sudo -H -u jenkins run-parts --exit-on-error --verbose $docker_workspace/tests/docker_scrips
