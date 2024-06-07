BOOT_SRC_DIR = $(SRC_DIR)/bootloader
BOOT_BIN_DIR = $(BIN_DIR)/bootloader
BOOT_BUILD_DIR = $(BUILD_DIR)/bootloader

BOOT_OUT_SO = $(BOOT_BIN_DIR)/bootloader.so
BOOT_OUT_EFI = $(BOOT_BIN_DIR)/bootx64.efi

BOOT_SRC = \
	$(wildcard $(BOOT_SRC_DIR)/*.c) \
	$(wildcard $(BOOT_SRC_DIR)/*.s) \
	$(STDLIB)/string/strcpy.c \
	$(STDLIB)/string/strcmp.c \
	$(STDLIB)/string/strlen.c \
	$(STDLIB)/string/memcmp.c

BOOT_OBJ = $(patsubst $(SRC_DIR)/%, $(BOOT_BUILD_DIR)/%.o, $(BOOT_SRC))

GNU_EFI = vendor/gnu-efi

BOOT_C_FLAGS = $(BASE_C_FLAGS) \
	-nostdlib \
	-fpic -ffreestanding \
	-fno-stack-protector -fno-stack-check \
	-fshort-wchar -mno-red-zone -Wno-array-bounds \
	-fno-stack-protector \
	-D__BOOTLOADER__ \
	-I$(GNU_EFI)/inc

$(BOOT_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(BOOT_C_FLAGS) -c -o $@ $<

$(BOOT_BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(ASM_FLAGS) $^ -o $@

$(BOOT_OUT_EFI): $(BOOT_OBJ)
	$(MKCWD)
	$(LD) -shared -Bsymbolic -L$(GNU_EFI)/x86_64/lib -L$(GNU_EFI)/x86_64/gnuefi -T$(GNU_EFI)/gnuefi/elf_x86_64_efi.lds $(GNU_EFI)/x86_64/gnuefi/crt0-efi-x86_64.o $(BOOT_OBJ) -o $(BOOT_OUT_SO) -lgnuefi -lefi
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BOOT_OUT_SO) $(BOOT_OUT_EFI)
	rm $(BOOT_OUT_SO)

BUILD += $(BOOT_OUT_EFI)