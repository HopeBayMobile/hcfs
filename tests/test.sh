#!/bin/bash
echo ======== ${BASH_SOURCE[0]} ========
date
set -x -e

local_repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
docker_workspace=/home/jenkins/workspace/HCFS

$local_repo/utils/setup_dev_env.sh -vm tests

# Start Swift server
mkdir -p $local_repo/tmp/swift_data
sudo rm -rf $local_repo/tmp/swift_data/*
sudo docker rm -f swift_test || true
SWIFT_ID=$(sudo docker run -d -v $local_repo/tmp/swift_data:/srv --name=swift_test -t aerofs/swift)
SWIFT_IP=$(sudo docker inspect --format '{{ .NetworkSettings.IPAddress }}' $SWIFT_ID)

# Start test slave
sudo docker rm -f hcfs_test 2>/dev/null || true
SLAVE_ID=$(sudo docker run --privileged -v $local_repo:/home/jenkins/workspace/HCFS \
        -d --name=hcfs_test --link swift_test \
        docker:5000/docker-unittest-stylecheck-slave bash -c "/usr/sbin/sshd -D -p 22")
SLAVE_IP=$(sudo docker inspect --format '{{ .NetworkSettings.IPAddress }}' $SLAVE_ID)

# Inject SSH key into test slave
key=$local_repo/tmp/id_rsa
[ ! -f $key ] && ssh-keygen -f $key -N ""
sudo docker exec -i hcfs_test /bin/bash -c 'cat >> /home/jenkins/.ssh/authorized_keys' < ${key}.pub

# Wait test slave ready
SSH="ssh -oStrictHostKeyChecking=no -i $key jenkins@$SLAVE_IP"
while ! $SSH true; do sleep 1; done

# Setup docker slave
$SSH sudo $docker_workspace/utils/setup_dev_env.sh -vm docker_slave

# Running auto test
$SSH "run-parts --exit-on-error --verbose $docker_workspace/tests/docker_scrips"
