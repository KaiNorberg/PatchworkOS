COMP_NAME = dynlink
COMP_VERSION = 1.0.0
COMP_TYPE = shared

CFLAGS += -fvisibility=hidden
LDFLAGS += -Bsymbolic

include $(COMP_DIR)/Make.comp.defaults
include $(COMP_DIR)/Make.comp.rules
