#!/bin/bash
## This script is for adding deb files to APT repository.
## Can be ran on APT-server


# get version of new packages
cd ~/debsrc
dcloud_gateway=$(ls | grep dcloud-gateway)
dcloudgatewayapi=$(ls | grep dcloudgatewayapi)
savebox_ver=$(ls | grep savebox | cut -d "." -f 3)
savebox=$(ls | grep savebox)
s3ql=$(ls | grep s3ql_)
s3ql_dbg=$(ls | grep s3ql-dbg)

# first compare md5sum of *.deb between debsrc folder and ftp server
sudo md5sum -c ~/debsrc/*.md5 > /dev/null
if [ $? != '0' ]
then
    echo "[ERROR] The content of upgrade debian files are inconsistent. Please download debian files from ftp server again and run add_deb_package.sh again"
    exit 1
fi
# remove old packages from apt-server db 
cd /var/packages/ubuntu
sudo reprepro remove precise s3ql s3ql-dbg savebox dcloud-gateway dcloudgatewayapi > /dev/null

# import new packages to apt-server
sudo reprepro includedeb precise ~/debsrc/*.deb > /dev/null

# check if new packages were imported successful
# check apt-server db
cd ~/debsrc
Packages=$(curl http://127.0.0.1/packages/ubuntu/dists/precise/main/binary-amd64/Packages 2> /dev/null | grep "s3ql\|dcloud-gateway\|dcloudgatewayapi\|savebox" | grep "Filename")

case "$Packages" in
*$dcloud_gateway*$dcloudgatewayapi*$s3ql*$s3ql_dbg*savebox*$savebox_ver*) ;; 
*) echo "[ERROR] Wrong version in apt-server db. Run add_deb_package.sh again."
   exit 1
;;
esac

# check md5sum of *.deb between apt-server and ftp server
sed -i 's#dcloud-gateway#/var/packages/ubuntu/pool/main/d/dcloud-gateway/dcloud-gateway#' *.md5
sed -i 's#dcloudgatewayapi#/var/packages/ubuntu/pool/main/d/dcloudgatewayapi/dcloudgatewayapi#' *.md5
sed -i 's#s3ql_#/var/packages/ubuntu/pool/main/s/s3ql/s3ql_#' *.md5
sed -i 's#s3ql-dbg#/var/packages/ubuntu/pool/main/s/s3ql/s3ql-dbg#' *.md5
sed -i 's#'$savebox'#/var/packages/ubuntu/pool/main/s/savebox/savebox_'$savebox_ver'_all.deb#' *.md5

sudo md5sum -c ~/debsrc/*.md5 > /dev/null
if [ $? != '0' ]
then
    echo "[ERROR] The content of upgrade debian files are inconsistent. Please download debian files from ftp server and run add_deb_package.sh again"
    exit 1
fi

rm ~/debsrc/*

echo "[Done] Add upgrade debian files successful"
