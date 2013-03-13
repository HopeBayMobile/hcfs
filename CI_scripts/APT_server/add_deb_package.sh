#!/bin/bash
## This script is for adding deb files to APT repository.
## Can be ran on APT-server

cd /var/packages/ubuntu
# First remvoe old packages
sudo reprepro remove precise s3ql s3ql-dbg savebox dcloud-gateway dcloudgatewayapi
# Then put newest packages 
sudo reprepro includedeb precise ~/debsrc/*.deb
rm ~/debsrc/*
