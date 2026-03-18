NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/$(COMP).so

CFLAGS += -fPIC
ASFLAGS += -fPIC
LDFLAGS +=

all: $(TARGET)

.PHONY: all

include Make.rules
