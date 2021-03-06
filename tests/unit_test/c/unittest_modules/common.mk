# Usage of common.mk:
#
# * To stop re-run failed unittest with valgrind:
#   make test VALGRIND=false

# mentioning default target first, finalizing it at end of file.
all :

ifndef tests_unit_test_c_unittest_modules_common_mk

mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))
current_dir := $(notdir $(patsubst %/,%,$(dir $(mkfile_path))))
HCFS_ROOT ?= $(realpath $(current_dir)/../../../..)

# Points to the root of Google Test, relative to where this file is.
# Remember to tweak this if you move this file.
GTEST_DIR = $(realpath $(HCFS_ROOT)/tests/unit_test/c/gtest-1.7.0)

# Flags passed to the preprocessor.
# Set Google Test's header directory as a system directory, such that
# the compiler doesn't generate warnings in Google Test headers.
CXXFLAGS += -isystem $(GTEST_DIR)/include
CPPFLAGS += $(EXTRACPPFLAGS)

# gcc will compile: *.c/*.cpp files as C and C++ respectively.
CC	=	gcc
CXX	=	g++

###########################################################################
# CCache
###########################################################################
include $(realpath $(HCFS_ROOT)/build/ccache.mk)

###########################################################################
# Compiling flags
###########################################################################
# Flags for gcc (We trace coverage of hcfs source only)
CFLAGS += -ftest-coverage -std=gnu11
# Flags for g++
CXXFLAGS += -fpermissive -std=gnu++11
# Flags passed to C and C++ compilers
CPPFLAGS += -g -Wall -Wextra -Wno-unused-parameter \
	    -pthread -fprofile-arcs \
	    -D_FILE_OFFSET_BITS=64 \
	    -DENABLE_DEDUP=0 \
	    -DENABLE_ENCRYPT=0 \
	    -DENABLE_COMPRESS=0 \
	    -D_ANDROID_ENV_ \
	    -DUNITTEST \

LDFLAGS += -lpthread -ldl -ljansson -lcrypto -lfuse -lsqlite3 -lrt

# Support  gcc4.9 color output
GCC_VERSION_GE_49 := $(shell expr `$(CC) -dumpversion | cut -f1-2 -d.` \>= 4.9)
ifeq "$(GCC_VERSION_GE_49)" "1"
	CPPFLAGS += -fdiagnostics-color=auto
endif

# All Google Test headers.  Usually you shouldn't change this
# definition.
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
		$(GTEST_DIR)/include/gtest/internal/*.h

# Usually you shouldn't tweak such internal variables, indicated by a
# trailing _.
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

###########################################################################
# HCFS setup rules
###########################################################################

all test: setup
setup:
	@$(realpath $(HCFS_ROOT)/utils)/setup_dev_env.sh -m unit_test

test:
ifndef GEN_TEST
	@MAKEFLAGS=${MAKEFLAGS//B} $(MAKE) test
else
	@find . -name "*.gcov" -delete
	gcovr -p -k --root=$(realpath $(HCFS_ROOT)/src) .
endif

define COMPILE
  $(eval SRC_DIR := $1)
  $(eval INC_DIR := $(addprefix -iquote,$(USER_DIR)))

  $(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(INC_DIR) $(CFLAGS) -c $$< -o $$@ -MMD -MF $$@.d

  $(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(INC_DIR) $(CXXFLAGS) -c $$< -o $$@ -MMD -MF $$@.d
endef

define ADDMODULE
  $(eval MD_NAME := $(notdir $(MD_PATH)))
  $(eval OBJ_DIR := $(MD_PATH)/obj)

  clean: clean-module-$(MD_NAME)
  .PHONY: clean-module-$(MD_NAME)
  clean-module-$(MD_NAME):
	rm -rf $(OBJ_DIR)/* $(MD_PATH)/*.gc*

  $(OBJ_DIR):
	mkdir -p $$@

  ###########################################################################
  # Builds gtest.a and gtest_main.a.
  ###########################################################################
  # For simplicity and to avoid depending on Google Test's
  # implementation details, the dependencies specified below are
  # conservative and not optimized.  This is fine as Google Test
  # compiles fast and for ordinary users its source rarely changes.

  $(OBJ_DIR)/gtest-all.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest-all.cc -o $$@ -lstdc++

  $(OBJ_DIR)/gtest_main.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest_main.cc -o $$@ -lstdc++

  $(OBJ_DIR)/gtest.a : $(OBJ_DIR)/gtest-all.o
	$(AR) $(ARFLAGS) $$@ $$^

  $(OBJ_DIR)/gtest_main.a : $(OBJ_DIR)/gtest-all.o $(OBJ_DIR)/gtest_main.o
	$(AR) $(ARFLAGS) $$@ $$^

  ###########################################################################
  # Auto maintain compiling and dependency
  ###########################################################################
  # Where to find user code.
  $(foreach U,\
    $(USER_DIR),\
    $(eval $(call COMPILE, $(U))))

endef

define ADDCASE
  ifeq ($(BATCH_UT),)
  $(eval T := $(strip $1))
  $(eval C := $(strip $2))
  $(eval CASE_PASS := $(MD_PATH)/obj/$(T)-$(C).pass)

  test: $(CASE_PASS)

  $(CASE_PASS): export RERUN_TIMEOUT := $(RERUN_TIMEOUT)
  $(CASE_PASS): $(MD_PATH)/$(T)
	@echo $$< --gtest_filter=$(C)
	@cd $(MD_PATH) && \
	  $$< --gtest_filter=$(C) \
	  --gtest_output=xml:$(OBJ_DIR)/test_detail_$(T)-$(C).xml \
	  || { if [ -f cleanup.sh ]; then ./cleanup.sh;fi; \
	  $$$${VALGRIND:=valgrind} $$< --gtest_filter=$(C); }
	@mkdir -p $(dir $(CASE_PASS))
	@touch $(CASE_PASS)
  endif
endef

define ADDTEST
  export GEN_TEST ?= 1
  export RERUN_TIMEOUT ?= $(RERUN_TIMEOUT)
  $(eval T := $(strip $1))
  $(eval OBJ_DIR := $(MD_PATH)/obj)
  $(eval OBJS := $(addprefix $(OBJ_DIR)/, $2))
  ifneq ($(MAKECMDGOALS),clean)
    -include $(OBJS:%.o=%.o.d) # Load dependency info for *existing* .o files
  endif

  # Build UT
  $(OBJS) $(OBJ_DIR)/gtest_main.a: | $(OBJ_DIR)
  all test: $(MD_PATH)/$(T)
  $(MD_PATH)/$(T): CPPFLAGS+=-iquote $(MD_PATH)/unittests
  $(MD_PATH)/$(T): VPATH+=$(MD_PATH)/unittests
  $(MD_PATH)/$(T): $(OBJS) $(OBJ_DIR)/gtest_main.a
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $$^ -o $$@ $(LDFLAGS)
  
  # Clean UT
  .PHONY: clean-ut-$(T)
  clean: clean-ut-$(T)
  clean-ut-$(T):
	@rm -vf $(MD_PATH)/$(T)

  # Load test case from UT
  ifeq ($(BATCH_UT),)
    $(foreach C,\
      $(shell [ -f $(MD_PATH)/$(T) ] && $(MD_PATH)/$(T) --gtest_list_tests | awk 'NR>1&&/^[^ ]/{prefix=$$1}; NR>1&&/^ /{print prefix$$1}'),\
      $(eval $(call ADDCASE,$(T),$(C))))
  else
    $(eval UT_PASS := $(MD_PATH)/obj/$(T).pass)
    test: $(UT_PASS)
    $(UT_PASS): $(MD_PATH)/$(T)
	@echo $$<
	@cd $(MD_PATH) && \
	  $$< --gtest_output=xml:$(OBJ_DIR)/test_detail_$(T).xml
	@touch $(UT_PASS)
    $(eval BATCH_UT := )
  endif
  RERUN_TIMEOUT :=
endef

.PHONY: setup clean all test

tests_unit_test_c_unittest_modules_common_mk := 1
endif # tests_unit_test_c_unittest_modules_common_mk
