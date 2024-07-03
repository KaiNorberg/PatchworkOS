GNU_EFI = lib/gnu-efi

BOOT_OUT_SO = bin/bootloader/boot.so
BOOT_OUT_EFI = bin/bootloader/bootx64.efi

BOOT_SRC = \
	$(wildcard src/bootloader/*.c) \
	$(wildcard src/bootloader/*.s) \
	src/stdlib/string.c

BOOT_OBJ = $(patsubst src/%, build/bootloader/%.o, $(BOOT_SRC))

BOOT_C_FLAGS = $(BASE_C_FLAGS) \
	-fpic -fno-stack-check \
	-fshort-wchar -mno-red-zone -Wno-array-bounds \
	-mno-mmx -mno-3dnow \
	-mno-80387 -mno-sse \
	-mno-sse2 -mno-sse3 \
	-mno-ssse3 -mno-sse4 \
	-D__BOOTLOADER__ \
	-D__EMBED__ \
	-Iinclude/bootloader \
	-I$(GNU_EFI)/inc

BOOT_ASM_FLAGS = $(BASE_ASM_FLAGS) \
	-D__EMBED__ \
	-Iinclude/bootloader \
	-I$(GNU_EFI)/inc

build/bootloader/%.c.o: src/%.c
	$(MKCWD)
	$(CC) $(BOOT_C_FLAGS) -c -o $@ $<

build/bootloader/%.s.o: src/%.s
	$(MKCWD)
	$(ASM) $(BOOT_ASM_FLAGS) $^ -o $@

$(BOOT_OUT_EFI): $(BOOT_OBJ)
	$(MKCWD)
	$(LD) -shared -Bsymbolic -L$(GNU_EFI)/x86_64/lib -L$(GNU_EFI)/x86_64/gnuefi -T$(GNU_EFI)/gnuefi/elf_x86_64_efi.lds $(GNU_EFI)/x86_64/gnuefi/crt0-efi-x86_64.o $(BOOT_OBJ) -o $(BOOT_OUT_SO) -lgnuefi -lefi
	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BOOT_OUT_SO) $(BOOT_OUT_EFI)
	rm $(BOOT_OUT_SO)

BUILD += $(BOOT_OUT_EFI)
