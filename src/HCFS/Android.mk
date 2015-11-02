LOCAL_PATH := $(call my-dir)

#LIBS_PATH := libs/$(TARGET_ARCH_ABI)
LOCAL_ALLOW_UNDEFINED_SYMBOLS := true

include $(CLEAR_VARS)
LOCAL_CFLAGS	:= -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -D__MULTI_THREAD
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
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE
include $(BUILD_STATIC_LIBRARY)
#include $(BUILD_SHARED_LIBRARY)

#include $(CLEAR_VARS)
#LOCAL_MODULE    := libcurl
#LOCAL_SRC_FILES := libs/libcurl.so
#include $(PREBUILT_SHARED_LIBRARY)
#
#include $(CLEAR_VARS)
#LOCAL_MODULE    := libssl
#LOCAL_SRC_FILES := libs/libssl.so
#include $(PREBUILT_SHARED_LIBRARY)
#
#include $(CLEAR_VARS)
#LOCAL_MODULE:= libcrypto
#LOCAL_SRC_FILES := libs/libcrypto.so
#include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := liblz4
LOCAL_C_INCLUDES := lz4/lz4.c \
                    lz4/lz4frame.c \
                    lz4/lz4hc.c \
                    lz4/xxhash.c \
                    programs/lz4io.c\

LOCAL_SRC_FILES := $(LOCAL_PATH)/lz4
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE
include $(BUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= libcurl
LOCAL_SRC_FILES := libs/libcurl.a
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= libssl
LOCAL_SRC_FILES := libs/libssl.a
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE:= libcrypto
LOCAL_SRC_FILES := libs/libcrypto.a
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_CFLAGS    := -D_FILE_OFFSET_BITS=64 -D_ANDROID_ENV_ -DENCRYPT_ENABLE=0 -DCOMPRESS_ENABLE=0 -DDEDUP_ENABLE=0
LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE
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
                   compress.c
 
LOCAL_STATIC_LIBRARIES := libfuse libcurl libssl libcrypto liblz4
#LOCAL_STATIC_LIBRARIES := libfuse
##LOCAL_SHARED_LIBRARIES := libcurl libssl libcrypto
include $(BUILD_EXECUTABLE)

