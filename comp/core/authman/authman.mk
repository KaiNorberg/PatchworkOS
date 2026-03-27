COMP_NAME = authman
COMP_VERSION = 1.0.0
COMP_TYPE = executable
COMP_DEPENDS = libstd libdraw

LDFLAGS += -ldraw

include $(COMP_DIR)/Make.comp.defaults
include $(COMP_DIR)/Make.comp.rules
