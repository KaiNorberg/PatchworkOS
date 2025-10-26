NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/bootx64.efi

SRC += $(wildcard src/libstd/functions/string/*)

CFLAGS += \
	$(CFLAGS_DISABLE_SIMD) \
	-fpic -ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fshort-wchar \
	-mno-red-zone \
	-D__BOOT__ \
	-Iinclude/boot \
	-Ilib/gnu-efi/inc \
	-Isrc/libstd

LDFLAGS += --gc-sections

all: $(TARGET)

.PHONY: all

include Make.rules
