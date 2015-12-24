TOP := $(dir $(lastword $(MAKEFILE_LIST)))
INIT_CCACHE := $(TOP).init_ccache
ifeq "$(CCACHE_MK_INCLUDED)" ""

  CCACHE := /usr/bin/ccache
  ifneq "$(wildcard $(CCACHE))" ""

    export USE_CCACHE := 1
    export NDK_CCACHE := $(CCACHE)
    export CCACHE_CPP2 := yes
    export CCACHE_COMPILERCHECK := content

    setup : $(INIT_CCACHE)

    $(INIT_CCACHE) :
	@$(CCACHE) -M 10G
	@touch $(INIT_CCACHE)

  endif

  .PHONY : setup

else
  setup :
endif
export CCACHE_MK_INCLUDED := 1
