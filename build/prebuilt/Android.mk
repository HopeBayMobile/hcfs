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

ifeq "$(DEVICE)" "AOSP-nougat-arm64"
include $(CLEAR_VARS)
LOCAL_MODULE    := libz
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(DEVICE)/system/libz.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libssl
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(DEVICE)/system/libssl.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libicuuc
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(DEVICE)/system/libicuuc.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libicui18n
LOCAL_SRC_FILES := $(LOCAL_PATH)/$(DEVICE)/system/libicui18n.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)
endif

ifeq "$(DEVICE)" "AOSP-nougat-arm64"
LOCAL_MODULE    := libcurl
LOCAL_SRC_FILES += $(LOCAL_PATH)/$(DEVICE)/system/libcurl.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_STATIC_LIBRARY)
else
include $(CLEAR_VARS)
LOCAL_MODULE    := libcurl
LOCAL_SRC_FILES += $(wildcard \
		   $(LOCAL_PATH)/$(DEVICE)/system/libcurl.so \
		   $(LOCAL_PATH)/$(DEVICE)/libcurl.so)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)
endif

endif
export INCLUDE_PREBUILT_LIB := 1
