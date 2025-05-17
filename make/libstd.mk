NOSTDLIB=1
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

# Crt files need special handling
CRT_SRC = $(wildcard src/libstd/platform/user/crt/*.s) 
CRT_OBJ = $(patsubst src/libstd/platform/user/crt/%.s, $(BINDIR)/%.o, $(CRT_SRC))
$(BINDIR)/%.o: src/libstd/platform/user/crt/%.s
	$(MKCWD)
	$(AS) $(ASFLAGS) $< -o $@

all: $(TARGET) $(CRT_OBJ)

.PHONY: all

include Make.rules
