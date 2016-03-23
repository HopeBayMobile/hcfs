DIRS := $(wildcard c/unittest_modules/*/.)
BUILDDIRS = $(DIRS:%=build-%)
CLEANDIRS = $(DIRS:%=clean-%)
TESTSUCCESS = $(DIRS:%=%/.success)

all: $(BUILDDIRS)
$(DIRS): $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) EXTRACPPFLAGS=$(EXTRACPPFLAGS) -C $(@:build-%=%)

clean: $(CLEANDIRS)
$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean
	@rm -f $(@:clean-%=%)/.success

test: $(BUILDDIRS) $(TESTSUCCESS)

$(TESTSUCCESS):
	$(MAKE) -C $(@:%/.success=%) test
	touch $@

.PHONY: subdirs $(DIRS)
.PHONY: subdirs $(BUILDDIRS)
.PHONY: subdirs $(CLEANDIRS)
.PHONY: all clean test
