LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := libjansson
LOCAL_SRC_FILES := $(LOCAL_PATH)/mylibs/$(TARGET_ARCH_ABI)/libjansson.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libsqlite
LOCAL_SRC_FILES := $(LOCAL_PATH)/mylibs/$(TARGET_ARCH_ABI)/libsqlite.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_MODULE    := HCFS_api
LOCAL_SRC_FILES := HCFS_api.c
LOCAL_SHARED_LIBRARIES = libjansson
LOCAL_C_INCLUDES = $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE     := socket_serv
LOCAL_CFLAGS     := -pie -fPIE
LOCAL_LDFLAGS    := -pie -fPIE
LOCAL_SRC_FILES  := socket_serv.c pin_ops.c utils.c hcfs_stat.c hcfs_sys.c 
LOCAL_SHARED_LIBRARIES = libsqlite
LOCAL_C_INCLUDES := $(LOCAL_PATH)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE     := api_test
LOCAL_CFLAGS     := -pie -fPIE
LOCAL_LDFLAGS    := -pie -fPIE
LOCAL_SRC_FILES  := test.c HCFS_api.c utils.c 
LOCAL_SHARED_LIBRARIES = libjansson
LOCAL_C_INCLUDES := $(LOCAL_PATH)
include $(BUILD_EXECUTABLE)
