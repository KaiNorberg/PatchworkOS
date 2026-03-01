include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

LDFLAGS += 

all: $(TARGET)

.PHONY: all

include Make.rules
