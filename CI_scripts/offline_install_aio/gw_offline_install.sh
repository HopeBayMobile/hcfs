#!/bin/bash

# This script is for gateway offline installation.
# It will be packed with StorageAppliance/ and apt_cache/ folders in a tar file.

# define a function
check_ok() {
    if [ $? -ne 0 ];
    then
        echo "Execution encountered an error."
        exit 0
    fi
}
#----------------------------------------------


echo "************************"
echo "Usage: ./gw_offline_install.sh <mode = dom/vm>"
echo "************************"

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# make sure input arguments are correct
    if [ $# -ne 1 ]
    then
        echo "Please give dom or vm as input argument"
        exit 1
    fi

# define parameters
    source build.conf
    MODE=$1
    DEBFILE="debsrc_StorageAppliance_"$GW_VERSION"_"$OS_CODE_NAME"_"$COMPONENT"_"$ARCH".tgz"
    #~ DEBPATCH="debpatch_StorageAppliance_"$GW_VERSION"_"$OS_CODE_NAME"_"$COMPONENT"_"$ARCH".tgz"
    INITPATH=$(pwd)

# remove tgz file for saving space
    #~ rm gateway_install*.tar
    
# mount apt cache to ramdisk for saving space
    mount -o bind /dev/shm $APTCACHEDIR

# copy deb files to proper location
    echo "        ***** Untar DEB files *****"
    tar -xzf $DEBFILE -C $APTCACHEDIR
    # FIXME - use dcloudgateway-patch DEB install instead
    #~ mkdir -p StorageAppliance/GatewayPatches/debsrc    # in case of debsrc does not exist.
    #~ tar -xzf $DEBPATCH -C StorageAppliance/GatewayPatches/debsrc
    rm $DEBFILE
    #~ rm $DEBPATCH

# remove /etc/apt/sources.list
    mv /etc/apt/sources.list  /etc/apt/sources.list.bak

#~ # update apt index
# add deb files to search source
cat > /etc/apt/sources.list.d/apt-cache.list <<EOF
deb file:// $APTCACHEDIR/
EOF
apt-get update
apt-get upgrade -y --force-yes		# upgrade packages, e.g. ntp

# run gateway installation
    mkdir -p /mnt/cloudgwfiles/COSA
    if [ $MODE = "dom" ]
    then
        cd /tmp/INSTALL_Gateway
	bash DOM_gw_run_this_first.sh
    fi

# install all packages of gateway 
    echo "        ***** apt-get install dcloud-gateway *****"
    apt-get install -y --force-yes dcloud-gateway
    check_ok
    apt-get -y --force-yes -f install
    
## FIXME
# clean up temp files to free up sda space.
    cd $INITPATH
    #~ rm -r StorageAppliance
    rm gw_offline_install.sh
    apt-get clean
    apt-get autoclean
    apt-get autoremove
    rm -r /usr/share/doc /usr/src
    rm /etc/apt/sources.list.d/apt-cache.list
    
echo "....."
echo "Installation of Gateway is completed."