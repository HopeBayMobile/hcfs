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
    MODE=$1
    VERSION="1.11.1"
    BUILDNUM=$2
    TARDEBFILE="gw_apt_deb_precise.tgz"
    OUTPUTFILE="gateway_install_pkg_"$VERSION"-"$BUILDNUM".tgz"
    APTCACHEDIR=/var/cache/apt/archives
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
        git pull
    fi

# build DCloudS3ql
    cd $INITPATH
    bash deb_builder.sh $VERSION $BUILDNUM
    # Move S3QL DEB file to /var/cache/apt/
    cp StorageAppliance/s3ql*.deb $APTCACHEDIR 

# Update "apt-get" index
    #-- Ovid Wu <ovid.wu@delta.com.tw> Mon, 06 Aug 2012 06:18:03 +0000
    sudo dpkg-scanpackages ${APTCACHEDIR} > ${APTCACHEDIR}/Packages
    sudo gzip -f ${APTCACHEDIR}/Packages
    echo "Creating apt cache sources list"
cat > /etc/apt/sources.list.d/apt-cache.list <<EOF
deb file:// $APTCACHEDIR/
EOF

# Install gateway once
    if [ $MODE = "full" ]
    then
        cd StorageAppliance/INSTALL_Gateway/
        bash install_gateway_all_in_one.sh
            # install mdadm
            apt-get -y --force-yes install mdadm
            # fix apt-get
            apt-get -f install
        cd $INITPATH
    fi

# Update "apt-get" index
    #-- Ovid Wu <ovid.wu@delta.com.tw> Mon, 06 Aug 2012 06:18:03 +0000
    sudo dpkg-scanpackages ${APTCACHEDIR} > ${APTCACHEDIR}/Packages
    sudo gzip -f ${APTCACHEDIR}/Packages

# Back up all DEB files in /var/cache/apt/archives/ with the source code
# tar the DEB packages into a file.
    echo "packing DEB files"
    cd $APTCACHEDIR
    tar -czf $TARDEBFILE *
    mv $TARDEBFILE $INITPATH
    cd $INITPATH

# tar an all in one pack
    echo "creating gateway installation package"
    tar -czf $OUTPUTFILE StorageAppliance/ $TARDEBFILE gw_offline_install.sh

# Done.
    echo "~~~ DONE!"
