#!/bin/bash
exec 2>&1 >> /tmp/hb_patch.log
! [ -f system/core/sdcard/HCFSvol.h ] && find ./hb_patch -type f -name '*.patch' | xargs -I {} bash -c "patch -p1 < {}"
