OSNAME = PatchworkOS

LDS = src/kernel/linker.ld
LD = ld
LDFLAGS = -T $(LDS) -Bsymbolic -nostdlib

KERNEL_OBJDIR := build/kernel
LIBK_OBJDIR := build/libk

SRCDIR := src
OBJDIR := build
BINDIR := bin
FONTSDIR := fonts
BOOTDIR := gnu-efi/x86_64/bootloader
OVMFDIR := OVMFbin

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

KERNEL_OBJS = $(call rwildcard,$(KERNEL_OBJDIR),*.o)
LIBK_OBJS = $(call rwildcard,$(LIBK_OBJDIR),*.o)

setup:
	@mkdir -p $(BINDIR)
	@mkdir -p $(SRCDIR)
	@mkdir -p $(OBJDIR)

buildimg:
	dd if=/dev/zero of=$(BINDIR)/$(OSNAME).img bs=512 count=9375
	mkfs -t vfat $(BINDIR)/$(OSNAME).img
	mmd -i $(BINDIR)/$(OSNAME).img ::/EFI
	mmd -i $(BINDIR)/$(OSNAME).img ::/EFI/BOOT
	mmd -i $(BINDIR)/$(OSNAME).img ::/KERNEL
	mmd -i $(BINDIR)/$(OSNAME).img ::/ROOT
	mmd -i $(BINDIR)/$(OSNAME).img ::/ROOT/FONTS
	cp $(BOOTDIR)/main.efi $(BOOTDIR)/bootx64.efi
	mcopy -i $(BINDIR)/$(OSNAME).img $(SRCDIR)/startup.nsh ::
	mcopy -i $(BINDIR)/$(OSNAME).img $(BOOTDIR)/bootx64.efi ::/EFI/BOOT
	mcopy -i $(BINDIR)/$(OSNAME).img $(BINDIR)/kernel.elf ::/KERNEL
	mcopy -i $(BINDIR)/$(OSNAME).img $(FONTSDIR)/zap-vga16.psf ::/ROOT/FONTS
	mcopy -i $(BINDIR)/$(OSNAME).img $(FONTSDIR)/zap-light16.psf ::/ROOT/FONTS

linkkernel:
	$(LD) $(LDFLAGS) -o $(BINDIR)/kernel.elf $(KERNEL_OBJS) $(LIBK_OBJS)

all:
	make setup

	@echo !==== BOOTLOADER
	@cd gnu-efi && make bootloader
	@echo !==== LIBC
	@cd src/libc && make all
	@echo !==== KERNEL
	@cd src/kernel && make all

	@echo !==== LINK KERNEL
	make linkkernel

	@echo !==== BUILDIMG
	make buildimg

run:
	qemu-system-x86_64 -drive file=$(BINDIR)/$(OSNAME).img -m 4G -cpu qemu64 -drive if=pflash,format=raw,unit=0,file="$(OVMFDIR)/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="$(OVMFDIR)/OVMF_VARS-pure-efi.fd" -net none
