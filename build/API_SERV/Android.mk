LOCAL_PATH := $(call my-dir)
BUILD_PATH := $(abspath $(LOCAL_PATH)/..)

include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_MODULE    := HCFS_api
LOCAL_SRC_FILES := $(addprefix ../../src/API/, HCFS_api.c)
LOCAL_SHARED_LIBRARIES = libjansson
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE     := hcfsapid
LOCAL_CFLAGS     := -pie -fPIE
LOCAL_LDFLAGS    := -pie -fPIE
LOCAL_SRC_FILES  := $(addprefix ../../src/API/, socket_serv.c pin_ops.c hcfs_stat.c hcfs_sys.c enc.c socket_util.c logger.c smart_cache.c minimal_apk.c)
LOCAL_SHARED_LIBRARIES = libsqlite libcrypto libzip
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE     := hcfsconf
LOCAL_CFLAGS     := -pie -fPIE
LOCAL_LDFLAGS    := -pie -fPIE
LOCAL_SRC_FILES  := $(addprefix ../../src/API/, hcfsconf.c enc.c logger.c)
LOCAL_SHARED_LIBRARIES = libcrypto
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE     := api_test
LOCAL_CFLAGS     := -pie -fPIE
LOCAL_LDFLAGS    := -pie -fPIE
LOCAL_SRC_FILES  := $(addprefix ../../src/API/, test.c HCFS_api.c socket_util.c)
LOCAL_SHARED_LIBRARIES = libjansson
include $(BUILD_EXECUTABLE)

include $(BUILD_PATH)/third_party/Android.mk
include $(BUILD_PATH)/prebuilt/Android.mk
