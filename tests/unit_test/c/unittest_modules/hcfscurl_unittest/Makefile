# A sample Makefile for building Google Test and using it in user
# tests.  Please tweak it to suit your environment and project.  You
# may want to move it to your project's root directory.
#
# SYNOPSIS:
#
#   make [all]  - makes everything.
#   make TARGET - makes the given target.
#   make clean  - removes all files generated by make.

HCFS_ROOT ?= $(realpath ../../../../..)
mkfile_path := $(realpath $(lastword $(MAKEFILE_LIST)))
MD_PATH := $(realpath $(dir $(mkfile_path)))
USER_DIR := $(realpath $(dir $(mkfile_path))/unittests) \
  $(realpath $(HCFS_ROOT)/src/HCFS) \

include $(realpath $(HCFS_ROOT)/tests/unit_test/c/unittest_modules/common.mk)

$(eval $(call ADDMODULE))

# All tests produced by this Makefile.

$(eval $(call ADDTEST, hcfscurl_unittest, \
  mock_function.o \
  hcfscurl.o \
  hcfscurl_unittest.o ))
