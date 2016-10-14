LOCAL_PATH := $(call my-dir)

#LIBS_PATH := libs/$(TARGET_ARCH_ABI)
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

include $(CLEAR_VARS)
LOCAL_CFLAGS	:= -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -D__MULTI_THREAD -ggdb -O0
LOCAL_MODULE    := libfuse
LOCAL_SRC_FILES := cuse_lowlevel.c \
                   fuse.c \
                   fuse_kern_chan.c \
                   fuse_loop.c \
                   fuse_loop_mt.c \
                   fuse_lowlevel.c \
                   fuse_mt.c \
                   fuse_opt.c \
                   fuse_session.c \
                   fuse_signals.c \
                   helper.c \
                   mount.c \
                   mount_util.c \
                   ulockmgr.c

LOCAL_C_INCLUDES := jni/fuse
#LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -ggdb -O0 -fuse-ld=mcld
#include $(BUILD_STATIC_LIBRARY)
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libcurl
LOCAL_SRC_FILES := libs/libcurl.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libssl
LOCAL_SRC_FILES := libs/libssl.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= libcrypto
LOCAL_SRC_FILES := libs/libcrypto.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= liblz4
LOCAL_SRC_FILES := libs/liblz4.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libsqlite
LOCAL_SRC_FILES := libs/libsqlite.so
include $(PREBUILT_SHARED_LIBRARY)

#include $(CLEAR_VARS)
#LOCAL_MODULE := liblz4
#LOCAL_C_INCLUDES := $(LOCAL_PATH)/lz4
#LOCAL_C_FILES := lz4/lz4.c \
#                 lz4/lz4frame.c \
#                 lz4/lz4hc.c \
#                 lz4/xxhash.c \
#                 programs/lz4io.c\
#
#LOCAL_MODULE_TAGS := optional
#LOCAL_CFLAGS += -pie -fPIE
#LOCAL_LDFLAGS += -pie -fPIE
#include $(BUILT_STATIC_LIBRARY)
#include $(BUILD_SHARED_LIBRARY)

#include $(CLEAR_VARS)
#LOCAL_MODULE:= libcurl
#LOCAL_SRC_FILES := libs/libcurl.a
#LOCAL_CFLAGS += -pie -fPIE
#LOCAL_LDFLAGS += -pie -fPIE
#include $(PREBUILT_STATIC_LIBRARY)
#
#include $(CLEAR_VARS)
#LOCAL_MODULE:= libssl
#LOCAL_SRC_FILES := libs/libssl.a
#LOCAL_CFLAGS += -pie -fPIE
#LOCAL_LDFLAGS += -pie -fPIE
#include $(PREBUILT_STATIC_LIBRARY)
#
#include $(CLEAR_VARS)
#LOCAL_MODULE:= libcrypto
#LOCAL_SRC_FILES := libs/libcrypto.a
#LOCAL_CFLAGS += -pie -fPIE
#LOCAL_LDFLAGS += -pie -fPIE
#include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CFLAGS    := -D_FILE_OFFSET_BITS=64 -D_ANDROID_ENV_ -DENCRYPT_ENABLE=0 -DCOMPRESS_ENABLE=0 -DDEDUP_ENABLE=0 -DSTAT_VFS_H="<fuse/sys/statvfs.h>"
LOCAL_CFLAGS += -pie -fPIE -ggdb -O0 -Wall -Wextra
LOCAL_LDFLAGS += -pie -fPIE -ggdb -O0
LOCAL_MODULE    := hcfs
#LOCAL_SRC_FILES := fusexmp.c
#LOCAL_SRC_FILES := ndkNative.cpp
LOCAL_SRC_FILES := mount_manager.c \
                   FS_manager.c \
                   dir_entry_btree.c \
                   api_interface.c \
                   logger.c \
                   meta_mem_cache.c \
                   file_present.c \
                   fuseop.c \
                   super_block.c \
                   utils.c \
                   hfuse_system.c \
                   metaops.c \
                   filetables.c \
                   hcfscurl.c \
                   hcfs_tocloud.c \
                   hcfs_fromcloud.c \
                   hcfs_cacheops.c \
                   hcfs_cachebuild.c \
                   hcfs_clouddelete.c \
                   lookup_count.c \
                   b64encode.c \
                   xattr_ops.c \
                   enc.c \
                   path_reconstruct.c \
                   compress.c \
                   pin_scheduling.c \
                   monitor.c \
                   dir_statistics.c \
                   parent_lookup.c \
                   objmeta.c \
                   dedup_table.c \
                   rebuild_parent_dirstat.c\ 
 
#LOCAL_STATIC_LIBRARIES := libfuse libcurl libssl libcrypto liblz4
#LOCAL_STATIC_LIBRARIES := libfuse
LOCAL_SHARED_LIBRARIES := libcurl libssl libcrypto liblz4 libsqlite libfuse
include $(BUILD_EXECUTABLE)

