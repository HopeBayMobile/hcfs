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
    cd ../../../StorageAppliance/DCloudS3ql
    # prepare [debian] foler
    cp -r $INITPATH/debian_templates/s3ql/debian ./
    # build dependencies
    #
    #-- Ovid Wu <ovidwu@gmail.com> Sat, 18 Aug 2012 22:19:06 +0800
    #
    # FIXME: make sure all required dependency of s3ql is downloaded to
    # /var/cache/apt/archive in order to build debian binary package
    sudo apt-get -y build-dep s3ql  # s3ql has to be installed before.
    # update change log
    dch -v $VERSION.$BUILD -m "Modified by CDS Team @ Delta Cloud."
    # build DEB file
    dpkg-buildpackage -rfakeroot -b
