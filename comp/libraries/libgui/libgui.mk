COMP_NAME = libgui
COMP_VERSION = 1.0.0
COMP_TYPE = shared
COMP_DEPENDS = libc libm libgfx libpng

LDFLAGS += -lm -lgfx -lpng

include $(COMP_DIR)/Make.comp.defaults
include $(COMP_DIR)/Make.comp.rules
