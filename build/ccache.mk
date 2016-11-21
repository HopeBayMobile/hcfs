ifeq "$(CCACHE_MK_INCLUDED)" ""
  override PATH := /usr/lib/ccache:/usr/bin/ccache:$(PATH)
  CCACHE_EXISTS := $(shell ccache -V 2>/dev/null)
  ifdef CCACHE_EXISTS
    export USE_CCACHE := 1
    export CCACHE_UMASK=000
    export NDK_CCACHE := $(shell which ccache)
    export CCACHE_CPP2 := yes

    $(shell ccache -M 50G > /dev/null)
  else
  all:
	@echo '>> Install ccache can speed up make time about 4x or higher.'
	@echo '>> For ubuntu 14,16, use `utils/setup_dev_env.sh  -m install_ccache` to install ccache'
  endif
endif
export CCACHE_MK_INCLUDED := 1
