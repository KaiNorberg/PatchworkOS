BOOT_SRC_DIR = $(SRC_DIR)/bootloader
BOOT_BIN_DIR = $(BIN_DIR)/bootloader
BOOT_BUILD_DIR = $(BUILD_DIR)/bootloader

BOOT_OUTPUT_SO = $(BOOT_BIN_DIR)/bootloader.so
BOOT_OUTPUT_EFI = $(BOOT_BIN_DIR)/bootx64.efi

BOOT_SOURCE = $(call recursive_wildcard, $(BOOT_SRC_DIR), *.c)
BOOT_SOURCE += $(call recursive_wildcard, $(BOOT_SRC_DIR), *.asm)

BOOT_OBJECTS = $(patsubst $(BOOT_SRC_DIR)/%, $(BOOT_BUILD_DIR)/%.o, $(BOOT_SOURCE))

BOOT_C_FLAGS = -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -I$(BOOT_SRC_DIR) -I$(GNU_EFI)/inc -Wall -Werror

GNU_EFI = vendor/gnu-efi

$(BOOT_BUILD_DIR)/%.c.o: $(BOOT_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(BOOT_C_FLAGS) -c -o $@ $<)

$(BOOT_BUILD_DIR)/%.asm.o: $(BOOT_SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(BOOT_OUTPUT_EFI): $(BOOT_OBJECTS)
	@echo "!====== BUILDING BOOTLOADER ======!"
	@mkdir -p $(@D)
	$(LD) -shared -Bsymbolic -L$(GNU_EFI)/x86_64/lib -L$(GNU_EFI)/x86_64/gnuefi -T$(GNU_EFI)/gnuefi/elf_x86_64_efi.lds $(GNU_EFI)/x86_64/gnuefi/crt0-efi-x86_64.o $(BOOT_OBJECTS) -o $(BOOT_OUTPUT_SO) -lgnuefi -lefi
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BOOT_OUTPUT_SO) $(BOOT_OUTPUT_EFI)
	@rm $(BOOT_OUTPUT_SO)

BUILD += $(BOOT_OUTPUT_EFI)