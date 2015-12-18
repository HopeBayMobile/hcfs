export USE_CCACHE := 1
export NDK_CCACHE := /usr/bin/ccache
export CCACHE_CPP2 := yes
export CCACHE_COMPILERCHECK := content
export NDK_PROJECT_PATH := .
export NDK_APPLICATION_MK := ./Application.mk

all:
	# ccache
	-/usr/bin/ccache -M 10G
	ndk-build
clean:
	ndk-build clean
