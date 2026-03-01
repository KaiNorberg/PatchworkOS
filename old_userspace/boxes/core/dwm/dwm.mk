include Make.defaults

TARGET := $(BINDIR)/$(BOX)

LDFLAGS += 

all: $(TARGET)

.PHONY: all

include Make.rules
