include Make.defaults

TARGET := $(BINDIR)/kernel

SRC += $(wildcard src/libstd/*.c) $(wildcard src/libstd/*.s) \
	$(wildcard src/libstd/common/*.c) $(wildcard src/libstd/common/*.s) \
	$(wildcard src/libstd/platform/kernel/*.c) $(wildcard src/libstd/platform/kernel/*.s)

CFLAGS += $(CFLAGS_DISABLE_SIMD) -fno-pic -mcmodel=kernel \
	-fno-stack-check -mno-red-zone -Wno-array-bounds \
	-fno-stack-protector \
	-D__KERNEL__

ASFLAGS += -D__KERNEL__ -Isrc/libstd

LDFLAGS += -T$(SRCDIR)/linker.lds

all: $(TARGET)

.PHONY: all

include Make.rules
