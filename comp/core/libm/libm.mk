COMP_NAME = libm
COMP_VERSION = 1.0.0
COMP_TYPE = shared
COMP_DEPENDS = 

CFLAGS := $(filter-out -lc,$(CFLAGS))

include $(COMP_DIR)/Make.comp.defaults
include $(COMP_DIR)/Make.comp.rules
