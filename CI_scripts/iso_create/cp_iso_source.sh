#!/bin/bash

ubuntu_iso="ISO/*.iso" # this iso file name is controlled by build.conf
source_dir="source_ubuntu"
dest_dir="dest_ubuntu"
rm -rf $source_dir
rm -rf $dest_dir
mkdir -p $source_dir
mkdir -p $dest_dir
mount -o loop $ubuntu_iso $source_dir # mount the iso content to source_dir (the iso content in the source_dir is read-only)
rsync -avH $source_dir $dest_dir # copy the iso content in the source_dir to the dest_dir (the command line means that the copy is including the hard links, hidden files, normal files, etc.)
umount $source_dir 
rm $ubuntu_iso # delete the iso in the ISO folder after the copy operation
rmdir $source_dir # delete the source_ubuntu folder after the copy operation
