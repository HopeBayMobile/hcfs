all :
-include ../ccache.mk
-include ../ndk-build.mk

export NDK_PROJECT_PATH := .
export NDK_OUT := ../obj
export NDK_LIBS_OUT := ../libs

all : setup
	ndk-build NDK_APPLICATION_MK=Application.mk TARGET_PLATFORM=android-21
clean : setup
	ndk-build NDK_APPLICATION_MK=Application.mk clean
