ifeq "$(CCACHE_MK_INCLUDED)" ""
  TOP := $(dir $(lastword $(MAKEFILE_LIST)))
  CCACHE := /usr/bin/ccache
  ifneq "$(wildcard $(CCACHE))" ""
    export USE_CCACHE := 1
    export NDK_CCACHE := $(CCACHE)
    export CCACHE_CPP2 := yes
    export CCACHE_COMPILERCHECK := content

    $(shell /usr/bin/ccache -M 12G > /dev/null)
  else
    $(info ### Yout are not using ccache. Installing ccache can speedup rebuild time about 4x ###)
  endif
endif
export CCACHE_MK_INCLUDED := 1
