include Make.defaults

TARGET := $(BINDIR)/$(PKG)

LDFLAGS += -lpatchwork

all: $(TARGET)

.PHONY: all

include Make.rules
