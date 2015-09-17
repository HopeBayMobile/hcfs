#!/bin/bash

flags=$-
set +x # Pause debug, enable if verbose on
[[ "$flags" =~ "x" ]] && flag_x="-x" || flag_x="+x"

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

# Initialize our own variables:
output_file=""
verbose=0

while getopts ":vm:" opt; do
    case $opt in
    m)
        setup_dev_env_mode=$OPTARG
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

echo ======== ${BASH_SOURCE[0]} mode $setup_dev_env_mode ========

if [ $verbose -eq 0 ]; then set +x; else set -x; fi

export local_repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"

function install_pkg {
    set +x
    for pkg in $packages;
    do
        if ! dpkg -s $pkg >/dev/null 2>&1; then
            install="$install $pkg"
        fi
    done
    if [ $verbose -eq 0 ]; then set +x; else set -x; fi
    if [ "$install $force_install" != " " ]; then
        sudo apt-get install -y $install $force_install
        packages=""
        install=""
        force_install=""
    fi
}

# Add NOPASSWD for user, required by functional test to replace /etc/hcfs.conf
if [ -n "$USER" -a "$USER" != "root" -a ! -f /etc/sudoers.d/50_${USER}_sh ]; then
    sudo grep -q "^#includedir.*/etc/sudoers.d" /etc/sudoers || (echo "#includedir /etc/sudoers.d" | sudo tee -a /etc/sudoers)
    ( umask 226 && echo "$USER ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/50_${USER}_sh )
fi

case "$setup_dev_env_mode" in
docker_slave )
    echo 'Acquire::http::Proxy "http://cache:8000";' | sudo tee /etc/apt/apt.conf.d/30autoproxy
    sudo sed -r -i"" "s/archive.ubuntu.com/free.nchc.org.tw/" /etc/apt/sources.list
    sudo sed -r -i"" "s/archive.ubuntu.com/free.nchc.org.tw/" /etc/apt/sources.list.d/proposed.list
    ;;&
unit_test | functional_test | docker_slave )
	# dev dependencies
	packages="\
	libattr1-dev \
	libfuse-dev \
	libcurl4-openssl-dev \
	liblz4-dev \
	libssl-dev \
	"
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
        force_install="$force_install ccache"
    fi
    export USE_CCACHE=1
    ;;&
unit_test | docker_slave )
    packages="$packages gcovr"
    ;;&
functional_test | docker_slave )
    packages="$packages python-pip"
    packages="$packages python-swiftclient"
    install_pkg
    sudo -H pip install -q -r $local_repo/tests/functional_test/requirements.txt
    ;;&
tests )
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
    ;;&
* )
    install_pkg
    ;;
esac

set $flag_x
