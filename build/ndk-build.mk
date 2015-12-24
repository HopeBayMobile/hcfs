ifeq "$(ANDROID_NDK_MK_INCLUDED)" ""

  TOP := $(dir $(lastword $(MAKEFILE_LIST)))

  # 1. from make argument
  # 2. from config
  NDK_CONFIG := $(TOP).ndk_build
  ifeq "$(wildcard $(NDK_BUILD))" ""
    -include $(NDK_CONFIG)
  endif
  # 3. from PATH
  ifeq "$(wildcard $(NDK_BUILD))" ""
    export NDK_BUILD := $(shell /usr/bin/which ndk-build)
    SKIP_ADD_NDK_PATH := 1
  endif

  setup : $(NDK_CONFIG)

  $(NDK_CONFIG) :
  ifeq "$(wildcard $(NDK_BUILD))" ""
	$(error filepath NDK_BUILD is not set)
  else
	@echo "export NDK_BUILD=$(NDK_BUILD)" > "$(NDK_CONFIG)"
  endif

  SHELL := /bin/bash
  ifneq "$(SKIP_ADD_NDK_PATH)" "1"
    PATH := $(PATH):$(dir $(NDK_BUILD))
  endif

  .PHONY : setup
else
  setup :
endif
ANDROID_NDK_MK_INCLUDED := 1
