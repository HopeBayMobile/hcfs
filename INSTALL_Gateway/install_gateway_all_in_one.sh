#!/bin/bash
# install_gateway_all_in_one.sh : will install S3ql, GUI api and GUI all in once.

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# Yen: The following packages should be installed via "dpkg -i" if we are using customized version
# For 11.04: libfuse-dev, fuse-utils, libfuse2
# I found that in ubuntu 12.04, we might need to first purge the default libfuse2 (which will also
# remove other packages including grub2) before installing the customized libfuse2, or otherwise
# the changes may not take effect. We will have to somehow verify this at some point for 11.04 and 12.04.

# install dependent packages via apt-get
apt-get install -y --force-yes squid3

# vvvvv-- install S3QL and its dependent packages ---------------------------------------------------
dpkg -i ../DCloudS3ql/debsrc/python-llfuse_0.37.1-2_amd64.deb 
dpkg -i ../DCloudS3ql/debsrc/cython_0.15.1-2_amd64.deb 
dpkg -i ../DCloudS3ql/debsrc/smartmontools_5.39.1+svn3124-2_amd64.deb
dpkg -i ../DCloudS3ql/debsrc/s3ql_1.12.0~natty1_amd64.deb

# create soft link for s3ql program
cp -rs /usr/bin/*s3ql* /usr/local/bin/
# ^^^^^-- install S3QL and its dependent packages ----------------------------------------------------

# vvvvv-- install GUI API -----------------------------------------------------------------------------
cd ../DCloudGateway
./gateway_api_install_script
# install kernel and fuse patches
dpkg -i kernel_fuse_patches/linux-image-2.6.38.8-gateway_2.6.38.8-gateway-10.00.Custom_amd64.deb
dpkg -i kernel_fuse_patches/linux-headers-2.6.38.8-gateway_2.6.38.8-gateway-10.00.Custom_amd64.deb
dpkg -i kernel_fuse_patches/fuse-utils_2.8.4-1.1ubuntu4_amd64.deb
dpkg -i kernel_fuse_patches/libfuse2_2.8.4-1.1ubuntu4_amd64.deb
dpkg -i kernel_fuse_patches/libfuse-dev_2.8.4-1.1ubuntu4_amd64.deb
# install squid3 proxy
CACHEDIR="/storage/http_proxy_cache/"
mkdir -p $CACHEDIR
cat > /etc/squid3/squid.conf << EOF
http_port 3128 transparent
acl localnet src 127.0.0.1/255.255.255.255
http_access allow all
http_access allow localnet
dns_nameservers 8.8.8.8  168.95.1.1

# cache_dir ufs <cache-dir> <cache size in MB> <# of L1 directories> <# of L2 directories>
cache_dir ufs $CACHEDIR 51200 64 128
EOF

chmod 777 $CACHEDIR

# add double check cache directory at each power on
cat >/etc/init.d/make_http_proxy_cache_dir <<EOF
mkdir $CACHEDIR
chmod 777 $CACHEDIR
EOF
chmod 777 /etc/init.d/make_http_proxy_cache_dir
cp -rs /etc/init.d/make_http_proxy_cache_dir /etc/rc2.d/S29make_http_proxy_cache_dir

echo "    Squid3 configuration has been written."
# ^^^^^-- install GUI API -----------------------------------------------------------------------------

# vvvvv-- install GUI --------------------------------------------------------------------------------
cd ../DCloudGatewayUI
./setup.sh
# ^^^^^-- install GUI --------------------------------------------------------------------------------

# add a private of APT repository server
cat >/etc/apt/sources.list.d/delta-server.list <<EOF
deb http://www.deltacloud.tw/packages/ubuntu/ natty main
deb-src http://www.deltacloud.tw/packages/ubuntu natty main
EOF

sleep 3
echo "Installation of Gateway is completed..."
