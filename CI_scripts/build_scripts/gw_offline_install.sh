#!/bin/bash -x

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
echo "Usage: ./gw_offline_install.sh"
echo "************************"

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
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
    #~ mount -o bind /dev/shm $APTCACHEDIR

# copy deb files to proper location
    echo "        ***** Untar DEB files *****"
    tar -xzf $DEBFILE -C $APTCACHEDIR
    rm $DEBFILE

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
    # *********************************************
    # Test whether COSA does not need it
    #~ mkdir -p /mnt/cloudgwfiles/COSA
    # *********************************************

# install all packages of gateway 
    echo "        ***** apt-get install dcloud-gateway *****"
    apt-get install -y --force-yes dcloud-gateway
    check_ok

# patch Ubuntu12.04 kernel, fuse and samba 3.6.6
cd /tmp/GatewayPatches/
# install samba 3.6.6
echo "        ***** Install Samba patches *****"
apt-get -y --force-yes -f install
./install-samba.sh

# install kernel and fuse patches
#~ echo "        ***** Install kernel and fuse patches *****"
#~ ./install-u1204.sh

# install ldap and ldap-samba for SaveBox
echo "        ***** ldap and ldap-samba patches *****"
## FIX ME - why dependency error here?
# avoid system asking keep local version
mv /etc/init/nmbd.conf /tmp/
mv /etc/init/smbd.conf /tmp/
apt-get -y --force-yes -f install   
## FIX ME - why dependency error here?
cd /tmp/GatewayPatches/install_ldap
./Install_ldap_samba.sh
## FIX ME - move back config files
mv /tmp/nmbd.conf /etc/init/
mv /tmp/smbd.conf /etc/init/
## FIX ME - move back config files

# clean up temp files to free up sda space.
    cd $INITPATH
    #~ rm -r StorageAppliance
    rm gw_offline_install.sh
    #~ apt-get clean
    #~ apt-get autoclean
    apt-get autoremove
    rm -r /usr/share/doc /usr/src /tmp/GatewayPatches/
    rm /etc/apt/sources.list.d/apt-cache.list
    sed -i 's/cd \/root\/;bash gw_offline_install.sh vm/#/' /etc/rc.local   # clean up first time install
    
echo "....."
echo "Installation of Gateway is completed."
echo "The system will be powered off in 5 seconds."
sleep 5
poweroff