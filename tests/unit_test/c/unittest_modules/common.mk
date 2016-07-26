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
CPPFLAGS += -isystem $(GTEST_DIR)/include $(EXTRACPPFLAGS)

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
CFLAGS += -ftest-coverage
# Flags for g++
CXXFLAGS += -fpermissive -Wno-missing-field-initializers -std=c++11
# Flags passed to C and C++ compilers
CPPFLAGS += -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable \
	    -pthread -fprofile-arcs \
	    -D_FILE_OFFSET_BITS=64 \
	    -DDEDUP_ENABLE=0 \
	    -DENCRYPT_ENABLE=1 \
	    -DCOMPRESS_ENABLE=0 \
	    -DARM_32bit_ \
	    -D_ANDROID_ENV_
LNFLAGS += -lstdc++ -lpthread -ldl -ljansson -lcrypto -lfuse -lsqlite3

# Support  gcc4.9 color output
GCC_VERSION_GE_49 := $(shell expr `gcc -dumpversion | cut -f1-2 -d.` \>= 4.9)
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
	$(MAKE) test
else
	gcovr -p -k --root=$(realpath $(HCFS_ROOT)/src/HCFS) .
endif

define COMPILE
  $(eval INC_DIR := $(addprefix -iquote,$(USER_DIR)))

  $(OBJ_DIR)/%.d: $1/%.c | $(OBJ_DIR)
	  $(CC) -MM -MT $$(@.d=.o) $(CPPFLAGS) $(INC_DIR) $(CFLAGS) $$< > $$@
  $(OBJ_DIR)/%.o: $1/%.c | $(OBJ_DIR)
	  $(CC) $(CPPFLAGS) $(INC_DIR) $(CFLAGS) -c $$< -o $$@

  $(OBJ_DIR)/%.o: $1/%.cc | $(OBJ_DIR)
	  $(CXX) $(CPPFLAGS) $(INC_DIR) $(CXXFLAGS) -c $$< -o $$@
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

  $(CASE_PASS): $(MD_PATH)/$(T)
	@echo $$< --gtest_filter=$(C)
	@cd $(MD_PATH) && \
	  $$< --gtest_filter=$(C) --gtest_output=xml:$(OBJ_DIR)/test_detail_$(T)-$(C).xml
	@touch $(CASE_PASS)
  endif
endef

define ADDTEST
  export GEN_TEST ?= 1
  $(eval T := $(strip $1))
  $(eval OBJ_DIR := $(realpath $(MD_PATH)/obj))
  $(eval OBJS := $(addprefix $(OBJ_DIR)/, $2))
  ifneq ($(MAKECMDGOALS),clean)
    -include $(OBJS:%.o=%.d) # Load dependency info for *existing* .o files
  endif

  # Build UT
  all test: $(MD_PATH)/$(T)
  $(MD_PATH)/$(T): CPPFLAGS+=-iquote $(MD_PATH)/unittests
  $(MD_PATH)/$(T): VPATH+=$(MD_PATH)/unittests
  $(MD_PATH)/$(T): $(OBJS) $(OBJ_DIR)/gtest_main.a
	  $(CXX) $(CPPFLAGS) $(CXXFLAGS) -g $$^ -o $$@ $(LNFLAGS)
  
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
endef

.PHONY: setup clean all test

tests_unit_test_c_unittest_modules_common_mk := 1
endif # tests_unit_test_c_unittest_modules_common_mk
