NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/bootx64.efi

SRC += $(wildcard src/libstd/functions/string/*)

CFLAGS += $(CFLAGS_DISABLE_SIMD) -fpic -fno-stack-check \
	-fshort-wchar -mno-red-zone -Wno-array-bounds \
	-D__BOOTLOADER__ \
	-Iinclude/bootloader -Ilib/gnu-efi/inc \
	-Isrc/libstd

all: $(TARGET)

.PHONY: all

include Make.rules
