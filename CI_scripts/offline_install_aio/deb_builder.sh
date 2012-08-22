#!/bin/bash
# build DEB from source code
# make sure input arguments are correct
    echo "************************"
    echo "Usage: ./deb_builder.sh <gw_version> <s3ql_version> <build_num>"
    echo "Usage: ./deb_builder.sh <version_num> <build_num>"
    echo "************************"
    if [ $# -ne 3 ] 
    then
        echo "Need to input gw_version_num s3ql_version_num and build_num."
        echo "e.g. ./deb_builder.sh 1.0.9 1.11.1 3598"
        exit 1
    fi

# define parameters
    GW_VERSION=$1
    S3QL_VERSION=$2
    BUILD=$3
    INITPATH=$(pwd)

# build DCloudS3ql
    cd StorageAppliance/DCloudS3ql
    # prepare [debian] foler
    cp -r $INITPATH/debian_templates/s3ql/debian ./
    # build dependencies 
    sudo apt-get -y build-dep s3ql  # s3ql has to be installed before.
    # update change log
    dch -v $S3QL_VERSION.$BUILD -m "Modified by CDS Team @ Delta Cloud."
    # build DEB file
    dpkg-buildpackage -rfakeroot -b

# build DCloudGateway (API)
    cd $INITPATH
    mkdir -p /tmp/pkg_DCloudGateway/tmp
    cp -r $INITPATH/debian_templates/DCloudGateway/DEBIAN /tmp/pkg_DCloudGateway
    cp -r $INITPATH/StorageAppliance/DCloudGateway/ /tmp/pkg_DCloudGateway/tmp
    # edit control file
cat > /tmp/pkg_DCloudGateway/DEBIAN/control << EOF
Package: dcloudgatewayapi
Version: $GW_VERSION.$BUILD
Section: base
Priority: optional
Architecture: amd64
Depends: python, python-setuptools, python-software-properties, curl, portmap, nfs-kernel-server, samba, ntpdate, chkconfig, traceroute, swift, apt-show-versions, squid3
Maintainer: ovid.wu@delta.com.tw 
Description: Package for gateway API 
EOF
    dpkg --build /tmp/pkg_DCloudGateway ./
