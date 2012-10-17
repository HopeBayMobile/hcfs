#!/bin/bash -x
echo "************************"
echo "Usage: ./build_gw_package.sh <git_branch> <build_num>"
echo "************************"

# define a function
check_ok() {
    if [ $? -ne 0 ];
    then
        echo "Execution encountered an error."
        exit 0
    fi
}
#----------------------------------------------


# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# make sure input arguments are correct
    if [ $# -ne 2 ]
    then
        echo "Need to input 2 arguments."
        exit 1
    fi

# define parameters
    source build.conf

    BRANCH=$1
    BUILDNUM=$2
    DEBFILE="debsrc_StorageAppliance_"$GW_VERSION"_"$OS_CODE_NAME"_"$COMPONENT"_"$ARCH".tgz"
    DEBPATCH="debpatch_StorageAppliance_"$GW_VERSION"_"$OS_CODE_NAME"_"$COMPONENT"_"$ARCH".tgz"
    OUTPUTTAR="gateway_install_pkg_"$GW_VERSION"_"$BUILDNUM"_"$OS_CODE_NAME"_"$BRANCH"_"$ARCH".tar"
    OUTPUTISO="gateway_install_"$GW_VERSION"_"$BUILDNUM"_"$OS_CODE_NAME"_"$BRANCH"_"$ARCH".iso"
    INITPATH=$(pwd)

# create commit log file
    cd $INITPATH/StorageAppliance
    git log -1 --format="%H" > $INITPATH/$COMMIT_LOG
    cd $INITPATH

# clean old DEB files at $APTCACHEDIR
    rm $APTCACHEDIR/*.deb

# Download DEB files from FTP and copy DEB files to /var/cache/apt/archives
    wget ftp://anonymous@$FTP_HOST/$SRC_HOME/$DEBFILE
    check_ok
    wget ftp://anonymous@$FTP_HOST/$SRC_HOME/$DEBPATCH
    check_ok
    wget ftp://anonymous@$FTP_HOST/$SRC_HOME/$COSA_DEB
    check_ok
    # download clean OS ISO from FTP
    wget ftp://anonymous@$FTP_HOST/$ISO_HOME/$UBUNTU_ISO
    check_ok
    cp savebox*.deb $APTCACHEDIR    # move COSA deb
    tar -xzf $DEBFILE -C $APTCACHEDIR
    mkdir -p $INITPATH/StorageAppliance/CI_scripts/iso_create/ISO
    mv $UBUNTU_ISO $INITPATH/StorageAppliance/CI_scripts/iso_create/ISO
    check_ok

# build DCloudS3ql and DCloudGateway (API)
    cd $INITPATH
    echo "test start\n"
    pwd
    echo "test end\n"
    echo "deb_builder.sh $GW_VERSION $S3QL_VERSION $BUILDNUM $DEBPATCH"
    bash deb_builder.sh $GW_VERSION $S3QL_VERSION $BUILDNUM $DEBPATCH
    check_ok
    # Move S3QL DEB file to /var/cache/apt/
    cp StorageAppliance/s3ql*.deb $APTCACHEDIR
    # copy gateway api DEB file to /var/cache/apt/
    cp dcloudgatewayapi*.deb $APTCACHEDIR
    # copy dcloud-gateway meta package to /var/cache/apt/
    cp dcloud-gateway*.deb $APTCACHEDIR

# Update "apt-get" index
    #-- Ovid Wu <ovid.wu@delta.com.tw> Mon, 06 Aug 2012 06:18:03 +0000
    dpkg-scanpackages ${APTCACHEDIR} > ${APTCACHEDIR}/Packages
    check_ok
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
    tar -cf $OUTPUTTAR $DEBFILE gw_offline_install.sh build.conf $COMMIT_LOG

# upload all_in_one installation package to the FTP
    wput $OUTPUTTAR ftp://anonymous@$FTP_HOST/$BUILD_PATH/$OUTPUTTAR & # upload in background

# clean old files
    rm debsrc_StorageAppliance*.tgz
    rm debpatch_StorageAppliance*.tgz
    
# upload DEB files to APT server
	# FIXME - currently is saved to a directory
	DEBSAVE="/tmp/deb_archive/"$GW_VERSION"_"$BUILDNUM
	mkdir -p $DEBSAVE
    mv StorageAppliance/s3ql*.deb $DEBSAVE
    mv dcloudgatewayapi*.deb $DEBSAVE
    mv dcloud-gateway*.deb $DEBSAVE
	mv savebox*.deb $DEBSAVE
	
# Build an ISO with auto-install OS
    rm -r $INITPATH/StorageAppliance/CI_scripts/iso_create/gateway_package  # delete old file
    mkdir -p $INITPATH/StorageAppliance/CI_scripts/iso_create/gateway_package
    mv $OUTPUTTAR $INITPATH/StorageAppliance/CI_scripts/iso_create/gateway_package
    cd $INITPATH/StorageAppliance/CI_scripts/iso_create/
    bash -x cp_iso_source.sh
    bash -x build_iso.sh $GW_VERSION"_"$BUILDNUM"_"$OS_CODE_NAME"_"$BRANCH"_"$ARCH

# upload all_in_one installation package to the FTP
	wput $OUTPUTISO ftp://anonymous@$FTP_HOST/$BUILD_PATH/$OUTPUTISO
    # clear old ISO
    rm $INITPATH/StorageAppliance/CI_scripts/iso_create/$OUTPUTISO
    
# Done.
    echo "~~~ DONE!"
