#!/bin/bash
echo ===============================
date
set -x -e

export local_repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
export docker_workspace=/home/jenkins/workspace/HCFS

if ! hash docker; then
    curl https://get.docker.com | sudo sh
fi

# Use local docker registry server
if ! grep -q docker:5000 /etc/default/docker; then
    echo 'DOCKER_OPTS="$DOCKER_OPTS --insecure-registry docker:5000"' | sudo tee -a /etc/default/docker
    sudo service docker restart
fi

docker rm -f hcfs_test 2>/dev/null || true
docker pull docker:5000/docker-unittest-stylecheck-slave
CID=$(docker run --privileged -v $local_repo:/home/jenkins/workspace/HCFS -d --name=hcfs_test docker:5000/docker-unittest-stylecheck-slave bash -c "/usr/sbin/sshd -D -p 22")
IP=$(docker inspect --format '{{ .NetworkSettings.IPAddress }}' $CID)

key=$local_repo/tests/.id_rsa
[ ! -f $key ] && ssh-keygen -f $key -N ""
docker exec -i hcfs_test /bin/bash -c 'cat >> /home/jenkins/.ssh/authorized_keys' < ${key}.pub

SSH="ssh -oStrictHostKeyChecking=no -i $key jenkins@$IP"
while ! $SSH true; do sleep 1; done
$SSH sudo $docker_workspace/tests/ci_scripts/fix_docker_permission.sh
$SSH $docker_workspace/tests/ci_scripts/ci.sh
