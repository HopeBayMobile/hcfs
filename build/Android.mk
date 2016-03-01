TOP_LOCAL_PATH := $(abspath $(call my-dir))
include $(CLEAR_VARS)

include $(TOP_LOCAL_PATH)/third_party/Android.mk
include $(TOP_LOCAL_PATH)/API_SERV/Android.mk
include $(TOP_LOCAL_PATH)/HCFS/Android.mk
include $(TOP_LOCAL_PATH)/HCFS_CLI/Android.mk
