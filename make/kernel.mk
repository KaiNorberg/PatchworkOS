include Make.defaults

TARGET := $(BINDIR)/kernel.elf

SRC += $(wildcard src/stdlib/*.c) $(wildcard src/stdlib/*.s) \
	$(wildcard src/stdlib/*/*.c) $(wildcard src/stdlib/*/*.s)

CFLAGS += $(CFLAGS_DISABLE_SIMD) -fno-pic -mcmodel=large \
	-fno-stack-check -mno-red-zone -Wno-array-bounds \
	-fno-stack-protector \
	-D__EMBED__

ASFLAGS += -D__EMBED__

LDFLAGS += -T$(SRCDIR)/linker.lds

all: $(TARGET)

.PHONY: all

include Make.rules
