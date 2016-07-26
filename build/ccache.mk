ifeq "$(CCACHE_MK_INCLUDED)" ""
  override PATH := /usr/lib/ccache:/usr/bin/ccache:$(PATH)
  CCACHE_EXISTS := $(shell ccache -V)
  ifdef CCACHE_EXISTS
    export USE_CCACHE := 1
    export CCACHE_UMASK=000
    export NDK_CCACHE := $(shell which ccache)
    export CCACHE_CPP2 := yes

    $(shell ccache -M 50G > /dev/null)
  else
    $(info ### Yout are not using ccache. Installing ccache can speedup rebuild time about 4x ###)
  endif
endif
export CCACHE_MK_INCLUDED := 1
