#!/bin/bash

echo ======== ${BASH_SOURCE[0]} ========
export local_repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"

function install_pkg {
    if ! dpkg -s $packages >/dev/null 2>&1; then
        sudo apt-get install -y $packages
    fi
}

set -e -x
# dev dependencies
packages="
libattr1-dev
libfuse-dev
libcurl4-openssl-dev
liblz4-dev
libssl-dev
"

# Add NOPASSWD for user, required by functional test to replace /etc/hcfs.conf
if [ -n "$USER" -a "$USER" != "root" -a ! -f /etc/sudoers.d/50_${USER}_sh ]; then
    sudo grep -q "^#includedir.*/etc/sudoers.d" /etc/sudoers || (echo "#includedir /etc/sudoers.d" | sudo tee -a /etc/sudoers)
    ( umask 226 && echo "$USER ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/50_${USER}_sh )
fi

case "$mode" in
unit_test )
    packages="$packages gcovr"
    install_pkg
    ;;
functional_test )
    packages="$packages python-pip"
    install_pkg
    sudo -H pip install -r $local_repo/tests/functional_test/requirements.txt
    ;;
tests )
    packages="$packages python-swiftclient"
    install_pkg

    # Install Docker
    if ! hash docker; then
        curl https://get.docker.com | sudo sh
    fi
    if ! grep -q docker:5000 /etc/default/docker; then
        echo 'DOCKER_OPTS="$DOCKER_OPTS --insecure-registry docker:5000"' \
            | sudo tee -a /etc/default/docker
        sudo service docker restart
    fi
    # Pull test image from local registry server
    sudo docker pull docker:5000/docker-unittest-stylecheck-slave
    ;;
* )
    install_pkg
    ;;
esac
