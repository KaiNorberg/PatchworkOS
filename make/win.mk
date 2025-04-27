include Make.defaults

TARGET := $(BINDIR)/libwin.a

SRC = $(wildcard src/win/*.c) $(wildcard src/win/*.s)

all: $(TARGET)

.PHONY: all

include Make.rules
