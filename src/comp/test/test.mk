include Make.defaults

TARGET := $(BINDIR)/$(COMP)

LDFLAGS +=

all: $(TARGET)

.PHONY: all

include Make.rules
