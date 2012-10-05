#!/bin/bash

ubuntu_iso="ubuntu-12.04-server-amd64.iso"
source_dir="source_ubuntu"
dest_dir="dest_ubuntu"
rm -rf $source_dir
rm -rf $dest_dir
mkdir -p $source_dir
mkdir -p $dest_dir
mount -o loop $ubuntu_iso $source_dir
rsync -avH $source_dir $dest_dir
umount $source_dir

