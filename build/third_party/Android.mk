ifeq "$(INCLUDE_THIRD_PARTY)" ""

include $(call all-subdir-makefiles)

endif
export INCLUDE_THIRD_PARTY := 1
