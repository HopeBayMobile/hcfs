
TARGETS = API CLI_utils pyhcfs HCFS
BUILD_TARGETS = $(addprefix build-,$(TARGETS))
CLEAN_TARGETS = $(addprefix clean-,$(TARGETS))
.PHONY: $(BUILD_TARGETS) $(CLEAN_TARGETS)


all: $(BUILD_TARGETS)
clean: $(CLEAN_TARGETS)

$(BUILD_TARGETS):
	$(MAKE) -C $(@:build-%=%)

$(CLEAN_TARGETS):
	$(MAKE) -C $(@:clean-%=%) clean
