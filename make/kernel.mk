NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/kernel

SRC += $(wildcard src/libstd/*.c) $(wildcard src/libstd/*.s) \
 	$(wildcard src/libstd/functions/**/*.c) $(wildcard src/libstd/functions/**/*.s) \
	$(wildcard src/libstd/common/*.c) $(wildcard src/libstd/common/*.s) \
	$(wildcard src/libstd/platform/kernel/*.c) $(wildcard src/libstd/platform/kernel/*.s) \
	$(wildcard src/libstd/platform/kernel/functions/**/*.c) $(wildcard src/libstd/platform/kernel/functions/**/*.s) \

CFLAGS += $(CFLAGS_DISABLE_SIMD) -fno-pic -mcmodel=kernel \
	-fno-stack-check -mno-red-zone -Wno-array-bounds \
	-fno-stack-protector \
	-Isrc/libstd \
	-D__KERNEL__ \
	-D__STDC_WANT_LIB_EXT1__=1

ASFLAGS += -D__KERNEL__ -Isrc/libstd

LDFLAGS += -T$(SRCDIR)/linker.lds

all: $(TARGET)

.PHONY: all

include Make.rules
