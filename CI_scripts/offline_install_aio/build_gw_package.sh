#!/bin/bash
echo "************************"
echo "Usage: ./build_gw_package.sh <mode = full/fast> <build_num>"
echo "************************"

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
    if [ $1 != "full" ] && [ $1 != "fast" ]
    then
        echo "mode option should be 'full' or 'fast'"
        exit 1
    fi

# define parameters
    source build.conf

    MODE=$1
    BUILDNUM=$2
    DEBFILE="debsrc_StorageAppliance_"$GW_VERSION"_"$OS_CODE_NAME"_"$BRANCH"_"$ARCH".tgz"
    DEBPATCH="debpatch_StorageAppliance_"$GW_VERSION"_"$OS_CODE_NAME"_"$BRANCH"_"$ARCH".tgz"
    OUTPUTFILE="gateway_install_pkg_"$GW_VERSION"_"$BUILDNUM"_"$OS_CODE_NAME"_"$BRANCH"_"$ARCH".tgz"
    INITPATH=$(pwd)    

# pull code from github
    if [ $MODE = "full" ];    then
        git clone https://github.com/Delta-Cloud/StorageAppliance.git
    else
        if [ ! -d StorageAppliance ]; then
            echo "StorageAppliance has not been downloaded, use full mode to try again."
            exit 1
        fi
        cd StorageAppliance
        git reset --hard HEAD
        git pull
        cd ..
    fi

# Download DEB files from FTP and copy DEB files to /var/cache/apt/archives
#~ /usr/bin/ftp -n $FTP_HOST <<END_SCRIPT
#~ quote USER $USER
#~ quote PASS $PASSWD
#~ bin
#~ passive
#~ prompt
#~ cd $SRC_HOME
#~ mget $DEBFILE
#~ mget $DEBPATCH  
#~ quit
#~ END_SCRIPT
    wget ftp://anonymous@$FTP_HOST/$SRC_HOME/$DEBFILE
    wget ftp://anonymous@$FTP_HOST/$SRC_HOME/$DEBPATCH
    tar -xzf $DEBFILE -C $APTCACHEDIR
    if [ $? != 0 ]   # error handling
    then
        echo "Error in unpackaing DEB packages."
        exit 1
    fi
    
# build DCloudS3ql
    cd $INITPATH
    bash deb_builder.sh $S3QL_VERSION $BUILDNUM
    # Move S3QL DEB file to /var/cache/apt/
    rm $APTCACHEDIR/s3ql*   # remove old s3ql deb files
    cp StorageAppliance/s3ql*.deb $APTCACHEDIR 

# Update "apt-get" index
    #-- Ovid Wu <ovid.wu@delta.com.tw> Mon, 06 Aug 2012 06:18:03 +0000
    sudo dpkg-scanpackages ${APTCACHEDIR} > ${APTCACHEDIR}/Packages
    sudo gzip -f ${APTCACHEDIR}/Packages

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
    
# Done.
    echo "~~~ DONE!"
