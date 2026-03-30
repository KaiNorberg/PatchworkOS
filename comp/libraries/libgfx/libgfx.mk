COMP_NAME = libgfx
COMP_VERSION = 1.0.0
COMP_TYPE = shared
COMP_DEPENDS = libc libm

LDFLAGS += -lm

include $(COMP_DIR)/Make.comp.defaults
include $(COMP_DIR)/Make.comp.rules
