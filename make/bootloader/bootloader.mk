BOOT_SRC_DIR = $(SRC_DIR)/bootloader
BOOT_BIN_DIR = $(BIN_DIR)/bootloader
BOOT_BUILD_DIR = $(BUILD_DIR)/bootloader

BOOT_OUTPUT_SO = $(BOOT_BIN_DIR)/bootloader.so
BOOT_OUTPUT_EFI = $(BOOT_BIN_DIR)/bootx64.efi

BOOT_OBJECTS = $(call objects_pathsubst,$(BOOT_SRC_DIR),$(BOOT_BUILD_DIR),.c)
BOOT_OBJECTS += $(call objects_pathsubst,$(BOOT_SRC_DIR),$(BOOT_BUILD_DIR),.s)

GNU_EFI = vendor/gnu-efi

$(BOOT_BUILD_DIR)/%.c.o: $(BOOT_SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(BOOT_C_FLAGS) -I$(GNU_EFI)/inc -I$(BOOT_SRC_DIR) -c -o $@ $<

$(BOOT_BUILD_DIR)/%.s.o: $(BOOT_SRC_DIR)/%.s
	@mkdir -p $(@D)
	$(ASM) $(ASM_FLAGS) $^ -o $@

$(BOOT_OUTPUT_EFI): $(BOOT_OBJECTS)
	@mkdir -p $(@D)
	$(LD) -shared -Bsymbolic -L$(GNU_EFI)/x86_64/lib -L$(GNU_EFI)/x86_64/gnuefi -T$(GNU_EFI)/gnuefi/elf_x86_64_efi.lds $(GNU_EFI)/x86_64/gnuefi/crt0-efi-x86_64.o $(BOOT_OBJECTS) -o $(BOOT_OUTPUT_SO) -lgnuefi -lefi
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BOOT_OUTPUT_SO) $(BOOT_OUTPUT_EFI)
	@rm $(BOOT_OUTPUT_SO)

BUILD += $(BOOT_OUTPUT_EFI)