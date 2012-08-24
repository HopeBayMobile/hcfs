#!/bin/bash
# install_gateway_all_in_one.sh : will install S3ql, GUI api and GUI all in once.

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# update apt-get index if not to install dom
if [ "$1" != "dom" ]; then
   apt-get update
fi

# install gateway API
apt-get install -y --force-yes dcloud-gateway
apt-get -f install
#~ cd ../DCloudGateway
#~ ./gateway_api_install_script
    #~ apt-get install -y --force-yes dcloudgatewayapi
# install S3QL
    #~ cd ../DCloudS3ql
    #~ ./install_s3ql_script
#~ FIXME
    # 1. append apt-server domain name
    # 2. sudo apt-key adv --recv-keys ...


# install samba 3.6.6
cd ../GatewayPatches
./install-samba.sh
# install kernel and fuse patches
./install-u1204.sh

# install gateway GUI
apt-get -f install
cd ../DCloudGatewayUI
./setup.sh

update-rc.d celeryd defaults

sleep 3
echo "Installation of Gateway is completed..."
