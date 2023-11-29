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
OVMFDIR := OVMFbin

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

KERNEL_OBJS = $(call rwildcard,$(KERNEL_OBJDIR),*.o)
LIBK_OBJS = $(call rwildcard,$(LIBK_OBJDIR),*.o)

setup:
	@mkdir -p $(BINDIR)
	@mkdir -p $(SRCDIR)
	@mkdir -p $(OBJDIR)
	
	@echo !==== BOOTLOADER
	@cd src/bootloader && make setup
	@echo !==== LIBC
	@cd src/libc && make setup
	@echo !==== KERNEL
	@cd src/kernel && make setup	

	@cd gnu-efi && make all

buildimg:
	dd if=/dev/zero of=$(BINDIR)/$(OSNAME).img bs=512 count=9375
	mkfs -t vfat $(BINDIR)/$(OSNAME).img
	mmd -i $(BINDIR)/$(OSNAME).img ::/EFI
	mmd -i $(BINDIR)/$(OSNAME).img ::/EFI/BOOT
	mmd -i $(BINDIR)/$(OSNAME).img ::/KERNEL
	mmd -i $(BINDIR)/$(OSNAME).img ::/FONTS
	mcopy -i $(BINDIR)/$(OSNAME).img $(SRCDIR)/startup.nsh ::
	mcopy -i $(BINDIR)/$(OSNAME).img $(BINDIR)/bootx64/bootx64.efi ::/EFI/BOOT
	mcopy -i $(BINDIR)/$(OSNAME).img $(BINDIR)/kernel.elf ::/KERNEL
	mcopy -i $(BINDIR)/$(OSNAME).img $(FONTSDIR)/zap-vga16.psf ::/FONTS
	mcopy -i $(BINDIR)/$(OSNAME).img $(FONTSDIR)/zap-light16.psf ::/FONTS

linkkernel:
	$(LD) $(LDFLAGS) -o $(BINDIR)/kernel.elf $(KERNEL_OBJS) $(LIBK_OBJS)

all:
	@echo !==== BOOTLOADER
	@cd src/bootloader && make all
	@echo !==== LIBC
	@cd src/libc && make all
	@echo !==== KERNEL
	@cd src/kernel && make all

	@echo !==== LINK KERNEL
	make linkkernel

	@echo !==== BUILDIMG
	make buildimg

clean:
	@rm -rf $(OBJDIR)
	@rm -rf $(BINDIR)
	@cd gnu-efi && make clean

run:
	qemu-system-x86_64 -drive file=$(BINDIR)/$(OSNAME).img -m 4G -cpu qemu64 -drive if=pflash,format=raw,unit=0,file="$(OVMFDIR)/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="$(OVMFDIR)/OVMF_VARS-pure-efi.fd" -net none