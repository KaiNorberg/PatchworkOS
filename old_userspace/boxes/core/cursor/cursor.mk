include Make.defaults

TARGET := $(BINDIR)/$(BOX)

LDFLAGS += -lpatchwork

all: $(TARGET)

.PHONY: all

include Make.rules
