ifeq "$(INCLUDE_THIRD_PARTY)" ""

third_party_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

include $(third_party_dir)/jansson/Android.mk

include $(CLEAR_VARS)
  LOCAL_MODULE := liblz4
  LOCAL_SRC_FILES := $(wildcard $(third_party_dir)/lz4/lz4.c)
  LOCAL_C_INCLUDES := $(third_party_dir)/lz4
include $(BUILD_SHARED_LIBRARY)

endif
export INCLUDE_THIRD_PARTY := 1
