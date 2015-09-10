DIRS := $(wildcard c/unittest_modules/*/.)
DIRS := c/unittest_modules/enc_compress/.
BUILDDIRS = $(DIRS:%=build-%)
CLEANDIRS = $(DIRS:%=clean-%)
TESTDIRS = $(DIRS:%=test-%)

all: $(BUILDDIRS)
$(DIRS): $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) EXTRACPPFLAGS=$(EXTRACPPFLAGS) -C $(@:build-%=%)

clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

test: all $(TESTDIRS)

$(TESTDIRS):
	$(MAKE) -C $(@:test-%=%) test

.PHONY: subdirs $(DIRS)
.PHONY: subdirs $(BUILDDIRS)
.PHONY: subdirs $(CLEANDIRS)
.PHONY: subdirs $(TESTDIRS)
.PHONY: all clean test
