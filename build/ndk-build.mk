ifeq "$(ANDROID_NDK_MK_INCLUDED)" ""

  TOP := $(dir $(lastword $(MAKEFILE_LIST)))
  NDK_CONFIG := $(TOP).ndk_build
  # 1. from make argument
  # 2. from config
  ifeq "$(wildcard $(NDK_BUILD))" ""
    -include $(NDK_CONFIG)
  endif
  # 3. from PATH
  ifeq "$(wildcard $(NDK_BUILD))" ""
    export NDK_BUILD := $(shell /usr/bin/which ndk-build)
  endif

  ifeq "$(wildcard $(NDK_BUILD))" ""
    $(error filepath NDK_BUILD is not set)
  else
    # Save NDK_BUILD variable
    $(shell echo "export NDK_BUILD=$(NDK_BUILD)" > $(NDK_CONFIG))
    # Export NDK_BUILD into path
    PATH := $(PATH):$(dir $(NDK_BUILD))
  endif

endif
ANDROID_NDK_MK_INCLUDED := 1
