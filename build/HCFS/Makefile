CCACHE := /usr/bin/ccache
ifneq ("$(wildcard $(CCACHE))","")
export USE_CCACHE := 1
export NDK_CCACHE := $(CCACHE)
export CCACHE_CPP2 := yes
export CCACHE_COMPILERCHECK := content
SET_CCACHE_SIZE := $(CCACHE) -M 10G
endif

export NDK_PROJECT_PATH := .

all:
	$(SET_CCACHE_SIZE)
	ndk-build NDK_APPLICATION_MK=./Application.mk
clean:
	ndk-build NDK_APPLICATION_MK=./Application.mk clean
	rm -rf libs/armeabi/
