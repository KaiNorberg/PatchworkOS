include Make.defaults

TARGET := $(BINDIR)/libstd.a

SRC = $(wildcard src/stdlib/*.c) $(wildcard src/stdlib/*.s) \
	$(wildcard src/stdlib/common/*.c) $(wildcard src/stdlib/common/*.s) \
	$(wildcard src/stdlib/platform/user/*.c) $(wildcard src/stdlib/platform/user/*.s)

ASFLAGS += -Isrc/stdlib

all: $(TARGET)

.PHONY: all

include Make.rules
