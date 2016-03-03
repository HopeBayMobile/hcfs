ifeq "$(INCLUDE_THIRD_PARTY)" ""
	third_party_dir := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
	include $(third_party_dir)/jansson/Android.mk
endif
export INCLUDE_THIRD_PARTY := 1
