#!/bin/bash
for i in `find /sys/fs/fuse/connections -name abort`; do echo 1 > $i;done
sudo fusermount -u /tmp/test_fuse || :
