# mentioning default target first, finalizing it at end of file.
all :

# Points to the root of Google Test, relative to where this file is.
# Remember to tweak this if you move this file.
GTEST_DIR = ../../gtest-1.7.0

# Where to find user code.
USER_DIR = $(realpath ../../../../../src/HCFS)

# Where to put objects
OBJ_DIR = obj

# Flags passed to the preprocessor.
# Set Google Test's header directory as a system directory, such that
# the compiler doesn't generate warnings in Google Test headers.
CPPFLAGS += -isystem $(GTEST_DIR)/include $(EXTRACPPFLAGS)

# Search paths for headers
CPPFLAGS += -iquote$(USER_DIR)
# Search paths for source
vpath	%.c	$(USER_DIR)

# gcc will compile: *.c/*.cpp files as C and C++ respectively.
CC	=	gcc
CXX	=	g++

###########################################################################
# CCache
###########################################################################
export PATH := /usr/lib/ccache:$(PATH)
export USE_CCACHE := 1

###########################################################################
# Compiling flags
###########################################################################
# Flags for gcc (We trace coverage of hcfs source only)
CFLAGS += -ftest-coverage
# Flags for g++
CXXFLAGS += -fpermissive -Wno-missing-field-initializers
# Flags passed to C and C++ compilers
CPPFLAGS += -g -Wall -Wextra -pthread -fprofile-arcs \
	    -D_FILE_OFFSET_BITS=64 \
	    -DDEDUP_ENABLE=0 \
	    -DENCRYPT_ENABLE=1 \
	    -DCOMPRESS_ENABLE=0 \
	    -DARM_32bit_ \
	    -D_ANDROID_ENV_

# Support  gcc4.9 color output
GCC_VERSION_GE_49 := $(shell expr `gcc -dumpversion | cut -f1-2 -d.` \>= 4.9)
ifeq "$(GCC_VERSION_GE_49)" "1"
	CPPFLAGS += -fdiagnostics-color
endif

###########################################################################
# Builds gtest.a and gtest_main.a.
###########################################################################
# All Google Test headers.  Usually you shouldn't change this
# definition.
GTEST_HEADERS = $(GTEST_DIR)/include/gtest/*.h \
		$(GTEST_DIR)/include/gtest/internal/*.h

# Usually you shouldn't tweak such internal variables, indicated by a
# trailing _.
GTEST_SRCS_ = $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h $(GTEST_HEADERS)

# For simplicity and to avoid depending on Google Test's
# implementation details, the dependencies specified below are
# conservative and not optimized.  This is fine as Google Test
# compiles fast and for ordinary users its source rarely changes.
$(OBJ_DIR)/gtest-all.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest-all.cc -o $@ -lstdc++

$(OBJ_DIR)/gtest_main.o : $(GTEST_SRCS_)
	$(CXX) $(CPPFLAGS) -I$(GTEST_DIR) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest_main.cc -o $@ -lstdc++

$(OBJ_DIR)/gtest.a : $(OBJ_DIR)/gtest-all.o
	$(AR) $(ARFLAGS) $@ $^

$(OBJ_DIR)/gtest_main.a : $(OBJ_DIR)/gtest-all.o $(OBJ_DIR)/gtest_main.o
	$(AR) $(ARFLAGS) $@ $^


###########################################################################
# Auto maintain compiling and dependency
###########################################################################
$(OBJ_DIR):
	mkdir $@

# pull in dependency info for *existing* .o files
include $(OBJS:%.o=%.d)

# compile and generate dependency info
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
	@$(CC) -MM $(CPPFLAGS) $(CFLAGS) $< > $(@:%.o=%.d)
	@sed -i"" -r -e ':a;$$!N;$$!ba;s# \\\n##g;s#.*:#$(OBJ_DIR)/&#' -e 'p;s/\\ //g' -e 's/.*: //' -e '$$s/ |$$/:\n/g' $(@:%.o=%.d)

$(OBJ_DIR)/%.o: %.cc | $(OBJ_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
	@$(CXX) -MM $(CPPFLAGS) $(CXXFLAGS) $< > $(@:%.o=%.d)
	@sed -i"" -r -e ':a;$$!N;$$!ba;s# \\\n##g;s#.*:#$(OBJ_DIR)/&#' -e 'p;s/\\ //g' -e 's/.*: //' -e '$$s/ |$$/:\n/g' $(@:%.o=%.d)


