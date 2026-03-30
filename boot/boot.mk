include Make.defaults

BOOT_SRC = $(ROOT_DIR)/boot/src
BOOT_BUILD = $(BUILD_DIR)/boot
BOOT_BIN = $(BIN_DIR)/boot

TARGET := $(BOOT_BIN)/bootx64.efi

all: $(TARGET)

.PHONY: all

BOOT_CFLAGS := -DNDEBUG -DEFI_FUNCTION_WRAPPER -D_BOOT_ -Ilib -Iinclude -isystem $(STAGING_DIR)/include -I$(VENDOR_DIR)/gnu-efi/inc -I$(VENDOR_DIR)/gnu-efi/inc/x86_64 -fpic -ffreestanding -fno-builtin -fno-builtin-memcpy -fno-builtin-memset -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone

BOOT_LDFLAGS := -shared -nostdlib -Bsymbolic -z norelro -L$(VENDOR_DIR)/gnu-efi/x86_64/lib -L$(VENDOR_DIR)/gnu-efi/x86_64/gnuefi -T$(VENDOR_DIR)/gnu-efi/gnuefi/elf_x86_64_efi.lds $(VENDOR_DIR)/gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o

$(BOOT_BUILD)/boot.o: $(BOOT_SRC)/boot.c
	$(MKCWD)
	@echo "  CC    $<"
	@$(CC) $(BOOT_CFLAGS) -c $< -o $@

$(TARGET): $(BOOT_BUILD)/boot.o
	$(MKCWD)
	@echo "  LD    $@ (EFI)"
	@$(LD) $(BOOT_LDFLAGS) $< -o $(BOOT_BUILD)/boot.so -lgnuefi -lefi
	@echo "  OBJCOPY $@"
	@objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BOOT_BUILD)/boot.so $@
	@rm -f $(BOOT_BUILD)/boot.so
