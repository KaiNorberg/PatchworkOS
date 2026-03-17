NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/$(COMP).so

LDFLAGS +=

all: $(TARGET)

.PHONY: all

include Make.rules
