OSNAME = PatchworkOS

LDS = src/kernel/linker.ld
LD = ld
LDFLAGS = -T $(LDS) -Bsymbolic -nostdlib

OBJDIR := build/kernel

SRCDIR := src/kernel
OBJDIR := build/kernel
BINDIR := bin
OVMFDIR := vendor/OVMFbin
OSROOTDIR := root

rwildcard=$(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

OBJS = $(call rwildcard,$(OBJDIR),*.o)

setup:
	@mkdir -p $(BINDIR)
	@mkdir -p $(SRCDIR)
	@mkdir -p $(OBJDIR)
	
	@echo !==== BOOTLOADER
	@cd src/bootloader && make setup
	@echo !==== KERNEL
	@cd src/kernel && make setup	
	@echo !==== GNU-EFI
	@cd vendor/gnu-efi && make all

buildimg:
	dd if=/dev/zero of=$(BINDIR)/$(OSNAME).img bs=512 count=9375
	mkfs -t vfat $(BINDIR)/$(OSNAME).img
	mmd -i $(BINDIR)/$(OSNAME).img ::/EFI
	mmd -i $(BINDIR)/$(OSNAME).img ::/EFI/BOOT
	mmd -i $(BINDIR)/$(OSNAME).img ::/KERNEL
	mmd -i $(BINDIR)/$(OSNAME).img ::/FONTS
	mmd -i $(BINDIR)/$(OSNAME).img ::/PROGRAMS
	mcopy -i $(BINDIR)/$(OSNAME).img $(BINDIR)/bootx64/bootx64.efi ::/EFI/BOOT
	mcopy -i $(BINDIR)/$(OSNAME).img $(BINDIR)/kernel/kernel.elf ::/KERNEL
	mcopy -i $(BINDIR)/$(OSNAME).img $(OSROOTDIR)/startup.nsh ::
	mcopy -i $(BINDIR)/$(OSNAME).img $(OSROOTDIR)/FONTS/zap-vga16.psf ::/FONTS
	mcopy -i $(BINDIR)/$(OSNAME).img $(OSROOTDIR)/FONTS/zap-light16.psf ::/FONTS
	mcopy -i $(BINDIR)/$(OSNAME).img $(BINDIR)/programs/test.elf ::/PROGRAMS

all:
	@echo !==== BOOTLOADER
	@cd src/bootloader && make all
	@echo !==== KERNEL
	@cd src/kernel && make all

	@echo !==== LIBC
	@cd src/libc && make all

	@echo !==== TEST PROGRAM
	@cd src/programs/test && make all

	@echo !==== BUILDIMG
	make buildimg

clean:
	@rm -rf $(OBJDIR)
	@rm -rf $(BINDIR)
	@cd vendor/gnu-efi && make clean

run:
	qemu-system-x86_64 -drive file=$(BINDIR)/$(OSNAME).img -m 1G -cpu qemu64 -drive if=pflash,format=raw,unit=0,file="$(OVMFDIR)/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="$(OVMFDIR)/OVMF_VARS-pure-efi.fd" -net none
