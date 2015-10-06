#!/bin/bash
echo ======== ${BASH_SOURCE[0]} ========
date
set -x -e

local_repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
docker_workspace=/home/jenkins/workspace/HCFS

sudo rm -rf $local_repo/utils/.setup_*
$local_repo/utils/setup_dev_env.sh -v -m docker_host

# Start test slave
sudo docker rm -f hcfs_test 2>/dev/null || true
sudo docker pull docker:5000/docker_hcfs_test_slave
SLAVE_ID=$(sudo docker run -d -t \
		--privileged \
		-v /tmp/ccache:/home/jenkins/.ccache \
		-v $local_repo:/home/jenkins/workspace/HCFS \
		-v /var/run/docker.sock:/var/run/docker.sock \
		-v /etc/localtime:/etc/localtime:ro \
		--name=hcfs_test \
		docker:5000/docker_hcfs_test_slave)
SLAVE_IP=$(sudo docker inspect --format '{{ .NetworkSettings.IPAddress }}' $SLAVE_ID)

# Inject SSH key into test slave
mkdir -p $local_repo/tmp
key=$local_repo/tmp/id_rsa
[ ! -f $key ] && ssh-keygen -f $key -N ""
sudo docker exec -i $SLAVE_ID /bin/bash -c 'cat >> /home/jenkins/.ssh/authorized_keys' < ${key}.pub

# Wait test slave ready
SSH="ssh -o UserKnownHostsFile=/dev/null -oStrictHostKeyChecking=no -i $key jenkins@$SLAVE_IP"
while ! $SSH true; do sleep 1; done

# Setup docker slave
$SSH $docker_workspace/utils/setup_dev_env.sh -vm docker_slave

# Running auto test
$SSH "run-parts --exit-on-error --verbose $docker_workspace/tests/docker_scrips"

sudo docker stop $SLAVE_ID
