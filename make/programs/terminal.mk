include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

LDFLAGS += -lpatchwork

all: $(TARGET)

.PHONY: all

include Make.rules
