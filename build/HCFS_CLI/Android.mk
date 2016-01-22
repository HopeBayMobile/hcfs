# Copyright (C) 2012 Seth Huang<seth.hg@gmail.com>
#
LOCAL_PATH := $(dir $(call this-makefile))
BUILD_PATH := $(abspath $(dir $(call this-makefile))/..)

#include $(CLEAR_VARS)

#LOCAL_CFLAGS	:= -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -D__MULTI_THREAD
#LOCAL_CFLAGS += -DFUSERMOUNT_DIR=\"/system/bin\" -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGE_FILES -D_REENTRANT -DFUSE_USE_VERSION=26

#LOCAL_MODULE    := libfuse
#LOCAL_C_INCLUDES := jni/include
#LOCAL_SRC_FILES := cuse_lowlevel.c fuse.c fuse_kern_chan.c fuse_loop.c fuse_loop_mt.c fuse_lowlevel.c fuse_mt.c fuse_opt.c fuse_session.c fuse_signals.c helper.c mount.c mount_util.c ulockmgr.c

#include $(BUILD_STATIC_LIBRARY)
#include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_CFLAGS	:= -D_FILE_OFFSET_BITS=64 -D_ANDROID_ENV_
LOCAL_CFLAGS	+= -pie -fPIE $(HCFS_CFLAGS)
LOCAL_LDFLAGS	+= -pie -fPIE
#LOCAL_C_INCLUDES := jni/include
LOCAL_MODULE    := HCFSvol
LOCAL_SRC_FILES := src/HCFSvol.c
#LOCAL_STATIC_LIBRARIES := libfuse
include $(BUILD_EXECUTABLE)
