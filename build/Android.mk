TOP_LOCAL_PATH := $(abspath $(call my-dir))
include $(CLEAR_VARS)

include third_party/jansson/Android.mk
include $(TOP_LOCAL_PATH)/API_SERV/Android.mk
include $(TOP_LOCAL_PATH)/HCFS/Android.mk
include $(TOP_LOCAL_PATH)/HCFS_CLI/Android.mk
