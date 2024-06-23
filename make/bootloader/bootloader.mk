GNU_EFI = deps/gnu-efi

BOOT_OUT_SO = $(BIN_DIR)/bootloader/boot.so
BOOT_OUT_EFI = $(BIN_DIR)/bootloader/bootx64.efi

BOOT_SRC = \
	$(wildcard $(SRC_DIR)/bootloader/*.c) \
	$(wildcard $(SRC_DIR)/bootloader/*.s) \
	$(SRC_DIR)/stdlib/string.c

BOOT_OBJ = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/bootloader/%.o, $(BOOT_SRC))

BOOT_C_FLAGS = $(BASE_C_FLAGS) \
	-fpic -fno-stack-check \
	-fshort-wchar -mno-red-zone -Wno-array-bounds \
	-mno-mmx -mno-3dnow \
	-mno-80387 -mno-sse \
	-mno-sse2 -mno-sse3 \
	-mno-ssse3 -mno-sse4 \
	-D__BOOTLOADER__ \
	-D__EMBED__ \
	-I$(INCLUDE_DIR)/bootloader \
	-I$(GNU_EFI)/inc

BOOT_ASM_FLAGS = $(BASE_ASM_FLAGS) \
	-D__EMBED__ \
	-I$(INCLUDE_DIR)/bootloader \
	-I$(GNU_EFI)/inc

$(BUILD_DIR)/bootloader/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(BOOT_C_FLAGS) -c -o $@ $<

$(BUILD_DIR)/bootloader/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(BOOT_ASM_FLAGS) $^ -o $@

$(BOOT_OUT_EFI): $(BOOT_OBJ)
	$(MKCWD)
	$(LD) -shared -Bsymbolic -L$(GNU_EFI)/x86_64/lib -L$(GNU_EFI)/x86_64/gnuefi -T$(GNU_EFI)/gnuefi/elf_x86_64_efi.lds $(GNU_EFI)/x86_64/gnuefi/crt0-efi-x86_64.o $(BOOT_OBJ) -o $(BOOT_OUT_SO) -lgnuefi -lefi
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BOOT_OUT_SO) $(BOOT_OUT_EFI)
	rm $(BOOT_OUT_SO)

BUILD += $(BOOT_OUT_EFI)
