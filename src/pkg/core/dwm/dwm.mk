include Make.defaults

TARGET := $(BINDIR)/$(PKG)

LDFLAGS += 

all: $(TARGET)

.PHONY: all

include Make.rules
