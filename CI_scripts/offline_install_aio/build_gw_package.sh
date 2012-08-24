#!/bin/bash
echo "************************"
echo "Usage: ./build_gw_package.sh <mode = full/fast> <git_branch> <build_num>"
echo "************************"

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# make sure input arguments are correct
    if [ $# -ne 3 ]
    then
        echo "Need to input 3 arguments."
        exit 1
    fi
    if [ $1 != "full" ] && [ $1 != "fast" ]
    then
        echo "mode option should be 'full' or 'fast'"
        exit 1
    fi

# define parameters
    source build.conf

    MODE=$1
    BRANCH=$2
    BUILDNUM=$3
    DEBFILE="debsrc_StorageAppliance_"$GW_VERSION"_"$OS_CODE_NAME"_"$COMPONENT"_"$ARCH".tgz"
    DEBPATCH="debpatch_StorageAppliance_"$GW_VERSION"_"$OS_CODE_NAME"_"$COMPONENT"_"$ARCH".tgz"
    OUTPUTFILE="gateway_install_pkg_"$GW_VERSION"_"$BUILDNUM"_"$OS_CODE_NAME"_"$BRANCH"_"$ARCH".tgz"
    INITPATH=$(pwd)

# pull code from github
    if [ $MODE = "full" ];    then
        git clone https://github.com/Delta-Cloud/StorageAppliance.git
        cd StorageAppliance
        git stash
        git checkout $BRANCH
        cd ..
    else
        if [ ! -d StorageAppliance ]; then
            echo "StorageAppliance has not been downloaded, use full mode to try again."
            exit 1
        fi
        cd StorageAppliance
        git stash
        git checkout $BRANCH
        git reset --hard HEAD
        git pull
        cd ..
    fi

# Download DEB files from FTP and copy DEB files to /var/cache/apt/archives
    wget ftp://anonymous@$FTP_HOST/$SRC_HOME/$DEBFILE
    wget ftp://anonymous@$FTP_HOST/$SRC_HOME/$DEBPATCH
    wget ftp://anonymous@$FTP_HOST/$SRC_HOME/$COSA_DEB
    mv savebox*.deb $APTCACHEDIR
    tar -xzf $DEBFILE -C $APTCACHEDIR
    if [ $? != 0 ]   # error handling
    then
        echo "Error in unpackaing DEB packages."
        exit 1
    fi

# build DCloudS3ql and DCloudGateway (API)
    cd $INITPATH
    bash deb_builder.sh $GW_VERSION $S3QL_VERSION $BUILDNUM
    # Move S3QL DEB file to /var/cache/apt/
    rm $APTCACHEDIR/s3ql*   # remove old s3ql deb files
    cp StorageAppliance/s3ql*.deb $APTCACHEDIR
    # copy gateway api DEB file to /var/cache/apt/
    cp dcloudgatewayapi*.deb $APTCACHEDIR
    # copy dcloud-gateway meta package to /var/cache/apt/
    cp dcloud-gateway*.deb $APTCACHEDIR

# Update "apt-get" index
    #-- Ovid Wu <ovid.wu@delta.com.tw> Mon, 06 Aug 2012 06:18:03 +0000
    dpkg-scanpackages ${APTCACHEDIR} > ${APTCACHEDIR}/Packages
    gzip -f ${APTCACHEDIR}/Packages

# Back up all DEB files in /var/cache/apt/archives/ with the source code
# tar the DEB packages into a file.
    echo "packing DEB files"
    cd $APTCACHEDIR
    tar -czf $DEBFILE *
    mv $DEBFILE $INITPATH
    cd $INITPATH

# tar an all in one pack
    echo "creating gateway installation package"
    tar -czf $OUTPUTFILE StorageAppliance/ $DEBFILE $DEBPATCH gw_offline_install.sh build.conf --exclude=StorageAppliance/.git

# clean old files
    rm debsrc_StorageAppliance*.tgz
    rm debpatch_StorageAppliance*.tgz
    rm StorageAppliance/s3ql*.deb
    rm dcloudgatewayapi*.deb

# Done.
    echo "~~~ DONE!"
