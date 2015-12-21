ifeq ($(CCACHE_MK_INCLUDED),)

CCACHE := /usr/bin/ccache
ifneq ($(wildcard $(CCACHE)),)
export USE_CCACHE := 1
export NDK_CCACHE := $(CCACHE)
export CCACHE_CPP2 := yes
export CCACHE_COMPILERCHECK := content
all : ccache

ccache :
	@$(CCACHE) -M 10G

.PHONY: ccache
endif

endif
export CCACHE_MK_INCLUDED := 1
