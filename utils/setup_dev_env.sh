#!/bin/bash

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

# Initialize our own variables:
output_file=""
verbose=0

while getopts ":vm:" opt; do
    case $opt in
    m)
        mode=$OPTARG
        ;;
    v)
        verbose=1
        ;;
    \?)
        echo "Invalid option: -$OPTARG" >&2
        exit 1
        ;;
    :)
        echo "Option -$OPTARG requires an argument." >&2
        exit 1
        ;;
    esac
done

if [ $verbose -ne 0 ]; then
    echo ======== ${BASH_SOURCE[0]} ========
    echo Setup mode: $mode
    set -x
fi

export local_repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"

function install_pkg {
    if ! dpkg -s $packages >/dev/null 2>&1; then
        sudo apt-get install -y $packages
        packages=""
    fi
}

# dev dependencies
packages="\
libattr1-dev \
libfuse-dev \
libcurl4-openssl-dev \
liblz4-dev \
libssl-dev \
"

# Add NOPASSWD for user, required by functional test to replace /etc/hcfs.conf
if [ -n "$USER" -a "$USER" != "root" -a ! -f /etc/sudoers.d/50_${USER}_sh ]; then
    sudo grep -q "^#includedir.*/etc/sudoers.d" /etc/sudoers || (echo "#includedir /etc/sudoers.d" | sudo tee -a /etc/sudoers)
    ( umask 226 && echo "$USER ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/50_${USER}_sh )
fi

case "$mode" in
unit_test )
    packages="$packages gcovr"
    # Use ccache to speedup compile
    if ! echo $PATH | grep -E "(^|:)/usr/lib/ccache(:|$)"; then
        export PATH="/usr/lib/ccache:$PATH"
    fi
    if ! ccache -V | grep 3.2; then
        if ! apt-cache policy ccache | grep 3.2; then
            source /etc/lsb-release
            echo "deb http://ppa.launchpad.net/comet-jc/ppa/ubuntu $DISTRIB_CODENAME main" \
                | sudo tee /etc/apt/sources.list.d/comet-jc-ppa-trusty.list
            sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 32EF5841642ADD17
            sudo apt-get update
        fi
        sudo apt-get install -y ccache
    fi
    export USE_CCACHE=1
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
