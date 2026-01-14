include Make.defaults

all: $(BINDIR)/bootx64.efi

.PHONY: all

$(BUILDDIR)/main.o: $(SRCDIR)/main.c
	$(MKCWD)
	@echo "  CC    $<"
	@gcc -DNDEBUG -DEFI_FUNCTION_WRAPPER -D_BOOT_ -Ilib -Iinclude -Iinclude/libstd -Isrc/libstd -Ilib/gnu-efi/inc -Ilib/gnu-efi/inc/x86_64 -fpic -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -c $(SRCDIR)/main.c -o $(BUILDDIR)/main.o

$(BINDIR)/bootx64.efi: $(BUILDDIR)/main.o
	$(MKCWD)
	@echo "  LD    $@ (EFI)"
	@ld -shared -nostdlib -fPIC -Bsymbolic -Llib/gnu-efi/x86_64/lib -Llib/gnu-efi/x86_64/gnuefi -Tlib/gnu-efi/gnuefi/elf_x86_64_efi.lds lib/gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o $(BUILDDIR)/main.o -o $(BUILDDIR)/main.so -lgnuefi -lefi
	@echo "  OBJCOPY $@"
	@objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BUILDDIR)/main.so $(BINDIR)/bootx64.efi
	@rm -f $(BINDIR)/main.so