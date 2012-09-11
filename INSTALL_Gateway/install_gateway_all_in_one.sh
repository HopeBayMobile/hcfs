#!/bin/bash
# install_gateway_all_in_one.sh : will install S3ql, GUI api and COSA all in once.

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

# update apt-get index if not to install dom
if [ "$1" != "dom" ]; then
   apt-get update
fi

# install gateway API
    echo "        ***** apt-get install dcloud-gateway *****"
    apt-get install -y --force-yes dcloud-gateway
    check_ok
    apt-get -y --force-yes -f install

# 1. append apt-server domain name
echo "        ***** append APT-server to /etc/apt/sources.list.d/ *****"
cat >/etc/apt/sources.list.d/delta-server-precise.list <<EOF
deb http://apt.delcloudia.com/packages/ubuntu/ precise main
deb-src http://apt.delcloudia.com/packages/ubuntu precise main
EOF

#~ FIXME
    # 2. sudo apt-key adv --recv-keys ...

# FIXME - Clean up unsed files to free up space
echo "        ***** clean up files *****"
rm -r /usr/share/doc /usr/src
rm -r /usr/local/MYSQL/mysql-test	## just a test, not sure whether it is safe.

# install samba 3.6.6
echo "        ***** Install Samba patches *****"
cd ../GatewayPatches
./install-samba.sh
# install kernel and fuse patches
echo "        ***** Install kernel and fuse patches *****"
./install-u1204.sh

# install gateway GUI
#~ echo "        ***** Install gateway GUI *****"
#~ apt-get -y --force-yes -f install
#~ cd ../DCloudGatewayUI
#~ ./setup.sh
#~ update-rc.d celeryd defaults

sleep 1
echo "Installation of Gateway is completed..."
