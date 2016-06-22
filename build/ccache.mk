ifeq "$(CCACHE_MK_INCLUDED)" ""
  CCACHE_EXISTS := $(shell ccache -V)
  ifdef CCACHE_EXISTS
    override PATH := /usr/lib/ccache:$(PATH)
    export USE_CCACHE := 1
    export CCACHE_BASEDIR=$(shell pwd)
    export CCACHE_UMASK=000
    export NDK_CCACHE := $(CCACHE)
    export CCACHE_CPP2 := yes
    export CCACHE_COMPILERCHECK := content

    $(shell ccache -M 50G > /dev/null)
  else
    $(info ### Yout are not using ccache. Installing ccache can speedup rebuild time about 4x ###)
  endif
endif
export CCACHE_MK_INCLUDED := 1
