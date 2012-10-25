#!/bin/bash
## This script is for adding deb files to APT repository.
## Can be ran on APT-server

cd /var/packages/ubuntu
sudo reprepro includedeb precise ~/debsrc/*.deb
rm ~/debsrc/*
