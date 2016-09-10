
TARGETS = API CLI_utils pyhcfs HCFS
BUILD_TARGETS = $(addprefix build-,$(TARGETS))
CLEAN_TARGETS = $(addprefix clean-,$(TARGETS))
.PHONY: $(BUILD_TARGETS) $(CLEAN_TARGETS)

export PARALLEL_JOBS=$(shell nproc)
ifeq ($(PARALLEL_JOBS), )
  export PARALLEL_JOBS=$(shell cat /proc/cpuinfo | grep processor | wc -l)
endif
export PARALLEL_JOBS:=-l $(PARALLEL_JOBS).5

all: $(BUILD_TARGETS)
clean: $(CLEAN_TARGETS)

$(BUILD_TARGETS):
	$(MAKE) $(PARALLEL_JOBS) -C $(@:build-%=%)

$(CLEAN_TARGETS):
	$(MAKE) $(PARALLEL_JOBS) -C $(@:clean-%=%) clean
