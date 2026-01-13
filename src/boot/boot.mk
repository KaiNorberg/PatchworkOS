NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/bootx64.efi

# We need the .elf file parsing stuff from libstd
SRC += $(call find_sources,src/libstd/functions/elf)

CFLAGS = \
	$(CFLAGS_DISABLE_SIMD) \
	-fpic \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fshort-wchar \
	-mno-red-zone \
	-D_BOOT_ \
	-Iinclude/boot \
	-Iinclude \
	-Iinclude/libstd \
	-Ilib/gnu-efi/inc \
	-Ilib \
	-Isrc/libstd \
	-DEFI_FUNCTION_WRAPPER \

LDFLAGS = 

ASFLAGS =

all: $(TARGET)

.PHONY: all

include Make.rules
