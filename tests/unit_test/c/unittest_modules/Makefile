HCFS_ROOT ?= $(realpath ../../../..)

ifdef EXTRACPPFLAGS
EXTRACPPFLAGS = EXTRACPPFLAGS=$(EXTRACPPFLAGS)
endif

include $(HCFS_ROOT)/tests/unit_test/c/unittest_modules/*/Makefile

.PHONY: all clean test
