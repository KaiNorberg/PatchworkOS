include Make.defaults

TARGET := $(BINDIR)/libstd.a

all: $(TARGET)

.PHONY: all

include Make.rules
