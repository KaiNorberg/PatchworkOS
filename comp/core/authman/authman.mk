COMP_NAME = authman
COMP_VERSION = 1.0.0
COMP_TYPE = executable
COMP_DEPENDS = libc libm libdraw freetype zlib libpng

LDFLAGS += -ldraw -lfreetype -lpng -lz -lm

include $(COMP_DIR)/Make.comp.defaults
include $(COMP_DIR)/Make.comp.rules
