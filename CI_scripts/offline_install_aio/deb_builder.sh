#!/bin/bash
# build DEB from source code

# define a function
check_ok() {
    if [ $? -ne 0 ];
    then
        echo "Execution encountered an error."
        exit 0
    fi
}
#----------------------------------------------

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
    echo "      ***** Building S3QL *****"
    GW_VERSION=$1
    S3QL_VERSION=$2
    BUILD=$3
    INITPATH=$(pwd)
    PY_BUILD_PATH="/tmp/pkg_DCloudGateway/usr/local/lib/python2.7/dist-packages/DCloudGateway"

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
    check_ok

# build DCloudGateway (API)
    cd $INITPATH
    rm -r /tmp/pkg_DCloudGateway/
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
    check_ok

#~ BUG in not fully achieve as setup.py install.
#~ # build DCloudGateway (API)
    #~ echo "      ***** Building dcloudgatewayapi *****"
    #~ cd $INITPATH
    #~ rm -r /tmp/pkg_DCloudGateway/
    #~ mkdir -p /tmp/pkg_DCloudGateway/tmp/
    #~ cp -r $INITPATH/debian_templates/DCloudGateway/DEBIAN /tmp/pkg_DCloudGateway
    #~ cp -r $INITPATH/StorageAppliance/DCloudGateway/config /tmp/pkg_DCloudGateway/tmp/
    #~ cp -r $INITPATH/StorageAppliance/DCloudGateway/gateway_scripts /tmp/pkg_DCloudGateway/tmp/
    #~ cp $INITPATH/StorageAppliance/DCloudGateway/src/http_proxy/gen_squid3_conf.sh /tmp/pkg_DCloudGateway/tmp/
    #~ cp -r $INITPATH/StorageAppliance/DCloudGateway/ /tmp/pkg_DCloudGateway/tmp

    #~ mkdir -p $PY_BUILD_PATH
    #~ # build api files
    #~ cd StorageAppliance/DCloudGateway
    #~ python setup.py clean
    #~ python setup.py build
    #~ python -m compileall build/lib.linux*     # compile python code
    #~ find build/ -type f -name "*.py" -exec rm -f {} \;   # clean python source code
    #~ cp -r build/lib.linux*/* $PY_BUILD_PATH
    #~ # edit control file
#~ cat > /tmp/pkg_DCloudGateway/DEBIAN/control << EOF
#~ Package: dcloudgatewayapi
#~ Version: $GW_VERSION.$BUILD
#~ Section: base
#~ Priority: optional
#~ Architecture: amd64
#~ Depends: python, python-setuptools, python-software-properties, curl, portmap, nfs-kernel-server, samba, ntpdate, chkconfig, traceroute, swift, apt-show-versions, squid3
#~ Maintainer: CDS Team <ctbd@delta.com.tw>
#~ Description: Package for gateway API
#~ EOF
    #~ dpkg --build /tmp/pkg_DCloudGateway $INITPATH
    #~ check_ok

# build dcloud-gateway meta package.
    echo "      ***** Building dcloud-gateway *****"
    cd $INITPATH
cat > $INITPATH/dcloud-gateway << EOF
# Source: <source package name; defaults to package name>
Section: main
Priority: optional
# Homepage: <enter URL here; no default>
Standards-Version: 3.9.2
#~
Package: dcloud-gateway
Version: $GW_VERSION.$BUILD
Maintainer: CDS Team <ctbd@delta.com.tw>
#Pre-Depends: curl, tofrodos
Depends: curl, tofrodos, savebox, dcloudgatewayapi (>=$GW_VERSION), s3ql (>=$S3QL_VERSION)
Architecture: amd64
# Copyright: <copyright file; defaults to GPL2>
# Changelog: <changelog file; defaults to a generic changelog>
Description: Cloud Gateway and SaveBox. Product of Delta Electronics, Inc.
EOF
equivs-build dcloud-gateway
