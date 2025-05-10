include Make.defaults

TARGET := $(BINDIR)/libstd.a

SRC = $(wildcard src/libstd/*.c) $(wildcard src/libstd/*.s) \
	$(wildcard src/libstd/common/*.c) $(wildcard src/libstd/common/*.s) \
	$(wildcard src/libstd/functions/**/*.c) $(wildcard src/libstd/functions/**/*.s) \
	$(wildcard src/libstd/platform/user/*.c) $(wildcard src/libstd/platform/user/*.s) \
	$(wildcard src/libstd/platform/user/common/*.c) $(wildcard src/libstd/platform/user/common/*.s) \
	$(wildcard src/libstd/platform/user/functions/**/*.c) $(wildcard src/libstd/platform/user/functions/**/*.s)

ASFLAGS += -Isrc/libstd

CFLAGS += -D__STDC_WANT_LIB_EXT1__=1

all: $(TARGET)

.PHONY: all

include Make.rules
