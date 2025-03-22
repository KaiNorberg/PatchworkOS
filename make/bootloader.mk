include Make.defaults

TARGET := $(BINDIR)/bootx64.efi

SRC += src/stdlib/string.c

CFLAGS += $(CFLAGS_DISABLE_SIMD) -fpic -fno-stack-check \
	-fshort-wchar -mno-red-zone -Wno-array-bounds \
	-D__BOOTLOADER__ \
	-Iinclude/bootloader -Ilib/gnu-efi/inc

all: $(TARGET)

.PHONY: all

include Make.rules
