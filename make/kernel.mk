include Make.defaults

TARGET := $(BINDIR)/kernel

SRC += $(wildcard src/stdlib/*.c) $(wildcard src/stdlib/*.s) \
	$(wildcard src/stdlib/common/*.c) $(wildcard src/stdlib/common/*.s) \
	$(wildcard src/stdlib/platform/kernel/*.c) $(wildcard src/stdlib/platform/kernel/*.s)

CFLAGS += $(CFLAGS_DISABLE_SIMD) -fno-pic -mcmodel=kernel \
	-fno-stack-check -mno-red-zone -Wno-array-bounds \
	-fno-stack-protector \
	-D__KERNEL__

ASFLAGS += -D__KERNEL__ -Isrc/stdlib

LDFLAGS += -T$(SRCDIR)/linker.lds

all: $(TARGET)

.PHONY: all

include Make.rules
