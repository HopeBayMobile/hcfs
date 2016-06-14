LOCAL_PATH := $(call my-dir)
ifeq "$(INCLUDE_PREBUILT_LIB)" ""

include $(CLEAR_VARS)
LOCAL_MODULE    := libcrypto
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(DEVICE)/system/libcrypto.so
ifdef OPENSSL_IS_BORINGSSL
    LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)/include/boringssl
else
    LOCAL_EXPORT_C_INCLUDES += $(LOCAL_PATH)/include/openssl
endif
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libsqlite
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(DEVICE)/system/libsqlite.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include/sqlite3
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libcurl
LOCAL_SRC_FILES += $(wildcard \
		   $(LOCAL_PATH)/$(DEVICE)/system/libcurl.so \
		   $(LOCAL_PATH)/$(DEVICE)/libcurl.so)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

endif
export INCLUDE_PREBUILT_LIB := 1
