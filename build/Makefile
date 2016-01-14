all :
-include ccache.mk
-include ndk-build.mk

export NDK_PROJECT_PATH := $(abspath .)
export NDK_OUT := $(abspath ./obj)
export NDK_LIBS_OUT := $(abspath ./libs)

all : setup
	ndk-build NDK_APPLICATION_MK=Application.mk TARGET_PLATFORM=android-21
clean : setup
	rm -rf libs obj
