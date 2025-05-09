include Make.defaults

TARGET := $(BINDIR)/libstd.a

SRC = $(wildcard src/libstd/*.c) $(wildcard src/libstd/*.s) \
	$(wildcard src/libstd/common/*.c) $(wildcard src/libstd/common/*.s) \
	$(wildcard src/libstd/platform/user/*.c) $(wildcard src/libstd/platform/user/*.s)

ASFLAGS += -Isrc/libstd

all: $(TARGET)

.PHONY: all

include Make.rules
