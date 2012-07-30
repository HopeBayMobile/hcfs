#!/bin/bash
# install_gateway_all_in_one.sh : will install S3ql, GUI api and GUI all in once.

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# install gateway API
cd ../DCloudGateway
./gateway_api_install_script
# install samba 3.6.6
cd ../GatewayPatches
cd samba-3.6.6-pkg
./install-samba.sh
# install kernel and fuse patches
cd ../ubuntu1204
./install-u1204.sh
cd ..

# install S3QL
cd ../DCloudS3ql
./install_s3ql_script

# install gateway GUI
cd ../DCloudGatewayUI
./setup.sh

update-rc.d celeryd defaults

sleep 3
echo "Installation of Gateway is completed..."
