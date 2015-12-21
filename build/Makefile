all :
-include ccache.mk
-include android-ndk.mk

export NDK_PROJECT_PATH := $(abspath .)
export NDK_OUT := $(abspath ./obj)
export NDK_LIBS_OUT := $(abspath ./libs)

all : setup
	ndk-build NDK_APPLICATION_MK=Application.mk

clean :
	rm -rf libs obj
