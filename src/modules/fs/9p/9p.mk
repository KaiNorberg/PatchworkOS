NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/$(MODULE)

CFLAGS += $(CFLAGS_MODULE)

ASFLAGS += $(ASFLAGS_MODULE)

LDFLAGS += $(LDFLAGS_MODULE)

all: $(TARGET)

.PHONY: all

include Make.rules
