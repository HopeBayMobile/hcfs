LOCAL_PATH := $(dir $(call this-makefile))
BUILD_PATH := $(abspath $(LOCAL_PATH)/..)
LIBS_PATH := $(BUILD_PATH)/prebuilt/$(DEVICE)

ifeq "$(INCLUDE_CRYPTO)" ""
  include $(CLEAR_VARS)
  LOCAL_MODULE    := libcrypto
  LOCAL_SRC_FILES := $(LIBS_PATH)/system/libcrypto.so
  include $(PREBUILT_SHARED_LIBRARY)
endif
export INCLUDE_CRYPTO := 1

ifeq "$(INCLUDE_SQLITE)" ""
  include $(CLEAR_VARS)
  LOCAL_MODULE    := libsqlite
  LOCAL_SRC_FILES := $(LIBS_PATH)/system/libsqlite.so
  include $(PREBUILT_SHARED_LIBRARY)
endif
export INCLUDE_SQLITE := 1

include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_MODULE    := HCFS_api
LOCAL_SRC_FILES := $(addprefix ../../src/API/, HCFS_api.c)
LOCAL_SHARED_LIBRARIES = libjansson
LOCAL_C_INCLUDES = $(BUILD_PATH)/include/jansson
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE     := hcfsapid
LOCAL_CFLAGS     := -pie -fPIE $(HCFS_CFLAGS)
LOCAL_LDFLAGS    := -pie -fPIE $(HCFS_LDFLAGS)
LOCAL_SRC_FILES  := $(addprefix ../../src/API/, socket_serv.c pin_ops.c utils.c hcfs_stat.c hcfs_sys.c enc.c)
LOCAL_SHARED_LIBRARIES = libsqlite libcrypto
LOCAL_C_INCLUDES := $(BUILD_PATH)/include/sqlite3
ifdef OPENSSL_IS_BORINGSSL
	LOCAL_C_INCLUDES += $(BUILD_PATH)/include/boringssl
else
	LOCAL_C_INCLUDES += $(BUILD_PATH)/include/openssl
endif
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE     := hcfsconf
LOCAL_CFLAGS     := -pie -fPIE $(HCFS_CFLAGS)
LOCAL_LDFLAGS    := -pie -fPIE $(HCFS_LDFLAGS)
LOCAL_SRC_FILES  := $(addprefix ../../src/API/, hcfsconf.c enc.c)
LOCAL_SHARED_LIBRARIES = libcrypto
ifdef OPENSSL_IS_BORINGSSL
	LOCAL_C_INCLUDES := $(BUILD_PATH)/include/boringssl
else
	LOCAL_C_INCLUDES := $(BUILD_PATH)/include/openssl
endif
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE     := api_test
LOCAL_CFLAGS     := -pie -fPIE $(HCFS_CFLAGS)
LOCAL_LDFLAGS    := -pie -fPIE $(HCFS_LDFLAGS)
LOCAL_SRC_FILES  := $(addprefix ../../src/API/, test.c HCFS_api.c utils.c)
LOCAL_SHARED_LIBRARIES = libjansson
LOCAL_C_INCLUDES = $(BUILD_PATH)/include/jansson
include $(BUILD_EXECUTABLE)
