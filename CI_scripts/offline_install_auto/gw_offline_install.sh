#!/bin/bash

# This script is for gateway offline installation.
# It will be packed with StorageAppliance/ and apt_cache/ folders in a tar file.

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
    MODE=$1

# remove tgz file for saving space
rm gateway_install*.tgz

APTCACHEDIR=/var/cache/apt/archives

# copy deb files to /var/cache/apt/archive
mkdir /tmp/debsrc
tar -xzf gw_apt_deb_precise.tgz -C /tmp/debsrc
mv /tmp/debsrc/* $APTCACHEDIR

# remove /etc/apt/sources.list
mv /etc/apt/sources.list  /etc/apt/sources.list.bak
#~ # update apt index
# add deb files to search source
cat > /etc/apt/sources.list.d/apt-cache.list <<EOF
deb file:// $APTCACHEDIR/
EOF
apt-get update

# run gateway installation
cd StorageAppliance/INSTALL_Gateway
if [ $MODE = "dom" ]
then
    bash DOM_gw_run_this_first.sh
    bash install_gateway_all_in_one.sh
else
    mkdir /storage
    bash install_gateway_all_in_one.sh
fi
