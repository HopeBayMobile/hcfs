-include ../ccache.mk
-include ../android-ndk.mk

export NDK_PROJECT_PATH := .
export NDK_OUT := ../obj
export NDK_LIBS_OUT := ../libs

all:
	ndk-build NDK_APPLICATION_MK=Application.mk
clean:
	ndk-build NDK_APPLICATION_MK=Application.mk clean
