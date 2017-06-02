LOCAL_PATH := $(call my-dir)

ifeq ($(BUILD_HCFS),)
include $(call all-subdir-makefiles)
endif

