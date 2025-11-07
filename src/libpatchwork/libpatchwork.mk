NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/libpatchwork.a

all: $(TARGET)

.PHONY: all

include Make.rules
