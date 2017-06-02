LOCAL_PATH := $(call my-dir)
BUILD_PATH := $(abspath $(LOCAL_PATH)/..)

include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_MODULE    := libhcfsapi
LOCAL_SRC_FILES := $(addprefix ../../src/API/, HCFS_api.c)
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_SHARED_LIBRARIES = libjansson
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE     := hcfsapid
LOCAL_CFLAGS     := -pie -fPIE
LOCAL_LDFLAGS    := -pie -fPIE
LOCAL_SRC_FILES  := $(addprefix ../../src/API/, socket_serv.c pin_ops.c hcfs_stat.c hcfs_sys.c enc.c socket_util.c logger.c smart_cache.c minimal_apk.c strrchr_chk.c strchr_chk.c vsprintf_chk.c strcat_chk.c)
ifeq "$(DEVICE)" "AOSP-nougat-arm64"
LOCAL_LDFLAGS   += -L/home/jiahong/AOSP_7.1_tera/out/target/product/tera-emulator-arm/system/lib64
endif
LOCAL_SHARED_LIBRARIES = libsqlite libcrypto libzip libicui18n libicuuc
LOCAL_C_INCLUDES += $(BUILD_PATH)/../src/include
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE     := hcfsconf
LOCAL_CFLAGS     := -pie -fPIE -O0
LOCAL_LDFLAGS   := -pie -fPIE -O0
LOCAL_SRC_FILES  := $(addprefix ../../src/API/, hcfsconf.c enc.c logger.c strrchr_chk.c strchr_chk.c vsprintf_chk.c strcat_chk.c strlen_chk.c)
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
