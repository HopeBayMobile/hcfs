ifeq "$(ANDROID_NDK_MK_INCLUDED)" ""

  TOP := $(dir $(lastword $(MAKEFILE_LIST)))
  NDK_CONFIG := $(TOP).ndk_dir

  define test_ndk_read_permission =
    ifneq "$$(NDK_DIR)" ""
      test = $$(info $$(shell ls $$(NDK_DIR) > /dev/null ))
    endif
  endef

# 1. Find correct ndk-build filepath from arguments

  ifneq "$(NDK_DIR)" ""
    override NDK_DIR := $(filter %/ndk-build, $(wildcard \
      $(NDK_DIR) $(NDK_DIR)/ndk-build ))
    override NDK_DIR := $(abspath $(dir $(NDK_DIR)))
    $(eval $(call test_ndk_read_permission))
  endif


# 2. from config
  ifeq "$(NDK_DIR)" ""
    -include $(NDK_CONFIG)
    $(eval $(call test_ndk_read_permission))
  endif

# 3. from PATH
  ifeq "$(NDK_DIR)" ""
    override NDK_DIR := $(abspath $(dir $(shell /usr/bin/which ndk-build)))
    $(eval $(call test_ndk_read_permission))
  endif

  ifeq "$(wildcard $(NDK_DIR))" ""
    $(error Need NDK_DIR to build lib. Set NDK_DIR before build: "export NDK_DIR=<path-of-android-ndk-r10e>")
  else
    # Save NDK_DIR variable
    $(shell echo "override NDK_DIR=$(NDK_DIR)" > $(NDK_CONFIG))
    # Export NDK_DIR into path
    override PATH := $(PATH):$(NDK_DIR)
  endif

endif
ANDROID_NDK_MK_INCLUDED := 1
