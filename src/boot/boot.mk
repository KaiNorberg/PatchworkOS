include Make.defaults

all: $(BINDIR)/bootx64.efi

.PHONY: all

BOOT_CFLAGS := -DNDEBUG -DEFI_FUNCTION_WRAPPER -D_BOOT_ -Ilib -Iinclude -Iinclude/libstd -Isrc/libstd -Ilib/gnu-efi/inc -Ilib/gnu-efi/inc/x86_64 -fpic -ffreestanding -fno-builtin -fno-builtin-memcpy -fno-builtin-memset -fno-tree-loop-distribute-patterns -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args

BOOT_LDFLAGS := -shared -nostdlib -fPIC -Bsymbolic -Llib/gnu-efi/x86_64/lib -Llib/gnu-efi/x86_64/gnuefi -Tlib/gnu-efi/gnuefi/elf_x86_64_efi.lds lib/gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o

$(BUILDDIR)/boot.o: $(SRCDIR)/boot.c
	$(MKCWD)
	@echo "  CC    $<"
	@gcc $(BOOT_CFLAGS) -c $< -o $@

$(BINDIR)/bootx64.efi: $(BUILDDIR)/boot.o
	$(MKCWD)
	@echo "  LD    $@ (EFI)"
	@ld $(BOOT_LDFLAGS) $< -o $(BUILDDIR)/boot.so -lgnuefi -lefi
	@echo "  OBJCOPY $@"
	@objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BUILDDIR)/boot.so $@
	@rm -f $(BINDIR)/boot.so