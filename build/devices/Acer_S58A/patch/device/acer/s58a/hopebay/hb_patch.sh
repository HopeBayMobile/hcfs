#!/bin/bash
[ -f system/core/sdcard/HCFSvol.h ] && find ./hb_patch -type f -name '*.patch' | xargs -I {} bash -c "patch -p1 < {}"
