NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/libdwm.a

SRC = $(wildcard src/libdwm/*.c) $(wildcard src/libdwm/*.s)

all: $(TARGET)

.PHONY: all

include Make.rules
