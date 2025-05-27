NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/libpatchwork.a

SRC = $(wildcard src/libpatchwork/*.c) $(wildcard src/libpatchwork/*.s)

all: $(TARGET)

.PHONY: all

include Make.rules
