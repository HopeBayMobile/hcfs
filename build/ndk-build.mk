ifeq "$(ANDROID_NDK_MK_INCLUDED)" ""

  TOP := $(dir $(lastword $(MAKEFILE_LIST)))
  NDK_CONFIG := $(TOP).ndk_dir

  # 1. Find correct ndk-build filepath from arguments
  override NDK_BUILD := $(filter %/ndk-build, $(wildcard \
	  $(NDK_DIR) \
	  $(NDK_DIR)/ndk-build \
	  $(NDK_BUILD) \
	  $(NDK_BUILD)/ndk-build))
  override NDK_DIR := $(abspath $(dir $(NDK_BUILD)))
  # 2. from config
  ifeq "$(NDK_DIR)" ""
    -include $(NDK_CONFIG)
  endif
  # 3. from PATH
  ifeq "$(NDK_DIR)" ""
    override NDK_DIR := $(abspath $(dir $(shell /usr/bin/which ndk-build)))
  endif

  ifeq "$(wildcard $(NDK_DIR))" ""
    $(error Usage: "make NDK_DIR=<path-of-android-ndk-r10e>" )
  else
    # Save NDK_DIR variable
    $(shell echo "override NDK_DIR=$(NDK_DIR)" > $(NDK_CONFIG))
    # Export NDK_DIR into path
    PATH := $(PATH):$(NDK_DIR)
  endif

endif
ANDROID_NDK_MK_INCLUDED := 1
