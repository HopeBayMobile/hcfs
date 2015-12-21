ifeq ($(ANDROID_NDK_MK_INCLUDED),)
TOP := $(dir $(lastword $(MAKEFILE_LIST)))
-include $(TOP)/.ndk_path
export PATH := $(NDK_PATH):$(PATH)

all : checkndk

checkndk :
ifeq ($(NDK_PATH),)
	@echo "Lack of NDK_PATH setting, please update Project/build/.ndk_path"
	@exit 1
endif


endif
export ANDROID_NDK_MK_INCLUDED := 1
