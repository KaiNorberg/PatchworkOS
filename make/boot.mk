NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/bootx64.efi

SRC += $(wildcard src/libstd/functions/string/*)

CFLAGS += $(CFLAGS_DISABLE_SIMD) -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args \
	-D__BOOTLOADER__ \
	-Iinclude/bootloader -Ilib/gnu-efi/inc \
	-Isrc/libstd

LDFLAGS += --gc-sections

all: $(TARGET)

.PHONY: all

include Make.rules
