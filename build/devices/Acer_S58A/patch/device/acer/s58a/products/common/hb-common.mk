# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
MY_LOCAL_PATH := device/acer/s58a

DEVICE_PACKAGE_OVERLAYS := $(MY_LOCAL_PATH)/hb_overlay $(DEVICE_PACKAGE_OVERLAYS)

BOARD_SEPOLICY_DIRS += \
    device/acer/s58a/hb_sepolicy
BOARD_SEPOLICY_UNION += \
    hcfsapid.te \
    hcfsd.te \
    terafonn_app.te \
    file_contexts \
    fs_use \
    seapp_contexts

# Hopebay Cloud Filesystem
PRODUCT_COPY_FILES += \
    $(MY_LOCAL_PATH)/hopebay/bin/hcfs:system/bin/hcfs \
    $(MY_LOCAL_PATH)/hopebay/bin/HCFSvol:system/bin/HCFSvol \
    $(MY_LOCAL_PATH)/hopebay/bin/hcfsapid:system/bin/hcfsapid \
    $(MY_LOCAL_PATH)/hopebay/bin/hcfsconf:system/bin/hcfsconf \
    $(MY_LOCAL_PATH)/hopebay/etc/hcfs.conf:system/etc/hcfs.conf \
    $(MY_LOCAL_PATH)/hopebay/lib64/libfuse.so:system/lib64/libfuse.so \
    $(MY_LOCAL_PATH)/hopebay/lib64/liblz4.so:system/lib64/liblz4.so \
    $(MY_LOCAL_PATH)/hopebay/lib64/libHCFS_api.so:system/lib64/libHCFS_api.so \
    $(MY_LOCAL_PATH)/hopebay/lib64/libterafonnapi.so:system/lib64/libterafonnapi.so \
    $(MY_LOCAL_PATH)/hopebay/lib64/libHCFS_api.so:system/lib64/libHCFS_api.so \
    $(MY_LOCAL_PATH)/hopebay/lib64/libjansson.so:system/lib64/libjansson.so \
    $(MY_LOCAL_PATH)/hopebay/root/init.hcfs.sh:root/init.hcfs.sh

# HCFS management app
PRODUCT_PACKAGES +=\
    HopebayHCFSmgmt
