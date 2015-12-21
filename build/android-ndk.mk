ifeq "$(ANDROID_NDK_MK_INCLUDED)" ""

  TOP := $(dir $(lastword $(MAKEFILE_LIST)))
  NDK_CONFIG := $(TOP).ndk_path
  -include $(NDK_CONFIG)

  ifeq "$(NDK_PATH)" ""
	export NDK_PATH := $(shell which ndk-build)
  endif
  ifneq "$(NDK_PATH)" ""
    export PATH := $(NDK_PATH):$(PATH)
  endif

  setup : $(NDK_CONFIG)

  $(NDK_CONFIG) :
  ifeq "$(NDK_PATH)" ""
	$(error NDK_PATH is not set)
  else
	@echo "export NDK_PATH:=$(NDK_PATH)" > "$(NDK_CONFIG)"
  endif

  .PHONY : setup
else
  setup :
endif
export ANDROID_NDK_MK_INCLUDED := 1
