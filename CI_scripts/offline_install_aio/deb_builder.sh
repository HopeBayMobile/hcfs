#!/bin/bash
# build DEB from source code
# make sure input arguments are correct
    echo "************************"
    echo "Usage: ./deb_builder.sh <version_num> <build_num>"
    echo "************************"
    if [ $# -ne 2 ] 
    then
        echo "Need to input version_num and build_num."
        echo "e.g. ./deb_builder.sh 1.11.1 3598"
        exit 1
    fi

# define parameters
    VERSION=$1
    BUILD=$2    
    INITPATH=$(pwd)
    
# build DCloudS3ql
    cd StorageAppliance/DCloudS3ql
    # prepare [debian] foler
    cp -r $INITPATH/debian_templates/s3ql/debian ./
    # build dependencies 
    sudo apt-get -y build-dep s3ql  # s3ql has to be installed before.
    # update change log
    dch -v $VERSION.$BUILD -m "Modified by CDS Team @ Delta Cloud."
    # build DEB file
    dpkg-buildpackage -rfakeroot -b

# build DCloudGateway (API)
    mkdir -p /tmp/pkg_DcloudGateway/tmp
    cp -r $INITPATH/StorageAppliance/DCloudGateway/DEBIAN /tmp/pkg_DcloudGateway
    cp -r $INITPATH/StorageAppliance/DCloudGateway/ /tmp/pkg_DcloudGateway/tmp
    cd /tmp
    dpkg --build pkg_DCloudGateway $INITPATH/
    
