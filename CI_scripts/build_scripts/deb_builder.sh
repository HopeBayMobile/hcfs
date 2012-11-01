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
    echo "Usage: ./deb_builder.sh <gw_version> <s3ql_version> <build_num> <DEBPATCH_file_name>"
    echo "************************"
    if [ $# -ne 4 ]
    then
        echo "Need to input gw_version_num s3ql_version_num, build_num and DEBPATCH_file_name."
        echo "e.g. ./deb_builder.sh 1.0.9 1.11.1 3598 DEBPATCH_file_name"
        exit 1
    fi

# define parameters
    echo "      ***** Building S3QL *****"
    GW_VERSION=$1
    S3QL_VERSION=$2
    BUILD=$3
    DEBPATCH=$4
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
    dpkg-buildpackage -rfakeroot -b -d
    check_ok

# build DCloudGatewayAPI
    cd $INITPATH
    rm -r /tmp/pkg_DCloudGatewayAPI/
    mkdir -p /tmp/pkg_DCloudGatewayAPI/tmp
    cp -r $INITPATH/debian_templates/DCloudGatewayAPI/DEBIAN /tmp/pkg_DCloudGatewayAPI
    cp -r $INITPATH/StorageAppliance/DCloudGateway/ /tmp/pkg_DCloudGatewayAPI/tmp
    # edit control file
# 2012/10/17, take out nfs-kernel-server and samba for fixing prompt screen. (Yen)
# 2012/10/19, WeiTang says nfs-kernel-server is not installed, thus install it back (Yen)
# 2012/10/31, move the dependency of nfs-kernel-server to dcloud-gateway package (Yen)
cat > /tmp/pkg_DCloudGatewayAPI/DEBIAN/control << EOF
Package: dcloudgatewayapi
Version: $GW_VERSION.$BUILD
Section: base
Priority: optional
Architecture: amd64
Depends: python, python-setuptools, python-software-properties, curl, portmap, ntpdate, chkconfig, traceroute, swift, apt-show-versions, squid3
Maintainer: CDS Team <ctbd@delta.com.tw>
Description: Package for gateway API
EOF
    dpkg --build /tmp/pkg_DCloudGatewayAPI .
    check_ok


# build dcloud-gateway meta package.
    echo "      ***** Building dcloud-gateway *****"
    cd $INITPATH
    rm -r /tmp/pkg_DCloudGateway/
    mkdir -p /tmp/pkg_DCloudGateway/tmp/GatewayPatches/debsrc
    cp -r $INITPATH/debian_templates/DCloudGateway/DEBIAN /tmp/pkg_DCloudGateway
    cp -r $INITPATH/StorageAppliance/INSTALL_Gateway /tmp/pkg_DCloudGateway/tmp
    cp -r $INITPATH/StorageAppliance/GatewayPatches /tmp/pkg_DCloudGateway/tmp
    tar -xzf $DEBPATCH -C /tmp/pkg_DCloudGateway/tmp/GatewayPatches/debsrc
    # edit control file
cat > /tmp/pkg_DCloudGateway/DEBIAN/control << EOF
Section: main
Priority: optional
Package: dcloud-gateway
Version: $GW_VERSION.$BUILD
Maintainer: CDS Team <ctbd@delta.com.tw>
Depends: nfs-kernel-server, curl, tofrodos, savebox, dcloudgatewayapi (>=$GW_VERSION), s3ql (>=$S3QL_VERSION)
Architecture: amd64
Description: Cloud Gateway and SaveBox. Product of Delta Electronics, Inc.
EOF
    dpkg --build /tmp/pkg_DCloudGateway .
    check_ok


## ----- old code -----
#~ PY_BUILD_PATH="/tmp/pkg_DCloudGatewayAPI/usr/local/lib/python2.7/dist-packages/DCloudGateway"
#~ BUG in not fully achieve as setup.py install.
#~ # build DCloudGateway (API)
    #~ echo "      ***** Building dcloudgatewayapi *****"
    #~ cd $INITPATH
    #~ rm -r /tmp/pkg_DCloudGatewayAPI/
    #~ mkdir -p /tmp/pkg_DCloudGatewayAPI/tmp/
    #~ cp -r $INITPATH/debian_templates/DCloudGateway/DEBIAN /tmp/pkg_DCloudGatewayAPI
    #~ cp -r $INITPATH/StorageAppliance/DCloudGateway/config /tmp/pkg_DCloudGatewayAPI/tmp/
    #~ cp -r $INITPATH/StorageAppliance/DCloudGateway/gateway_scripts /tmp/pkg_DCloudGatewayAPI/tmp/
    #~ cp $INITPATH/StorageAppliance/DCloudGateway/src/http_proxy/gen_squid3_conf.sh /tmp/pkg_DCloudGatewayAPI/tmp/
    #~ cp -r $INITPATH/StorageAppliance/DCloudGateway/ /tmp/pkg_DCloudGatewayAPI/tmp

    #~ mkdir -p $PY_BUILD_PATH
    #~ # build api files
    #~ cd StorageAppliance/DCloudGateway
    #~ python setup.py clean
    #~ python setup.py build
    #~ python -m compileall build/lib.linux*     # compile python code
    #~ find build/ -type f -name "*.py" -exec rm -f {} \;   # clean python source code
    #~ cp -r build/lib.linux*/* $PY_BUILD_PATH
    #~ # edit control file
#~ cat > /tmp/pkg_DCloudGatewayAPI/DEBIAN/control << EOF
#~ Package: dcloudgatewayapi
#~ Version: $GW_VERSION.$BUILD
#~ Section: base
#~ Priority: optional
#~ Architecture: amd64
#~ Depends: python, python-setuptools, python-software-properties, curl, portmap, nfs-kernel-server, samba, ntpdate, chkconfig, traceroute, swift, apt-show-versions, squid3
#~ Maintainer: CDS Team <ctbd@delta.com.tw>
#~ Description: Package for gateway API
#~ EOF
    #~ dpkg --build /tmp/pkg_DCloudGatewayAPI $INITPATH
    #~ check_ok
