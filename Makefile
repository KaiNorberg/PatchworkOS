OSNAME = PatchworkOS

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
	
	@echo !==== GNU-EFI
	@cd vendor/gnu-efi && make all

	@echo !==== BOOTLOADER ====!
	@cd src/bootloader && make -s setup
	
	@echo !==== KERNEL ====!
	@cd src/kernel && make -s setup

	@echo !==== LIBC ====!
	@cd src/libc && make -s setup

	@echo !==== TEST PROGRAM ====!
	@cd src/programs/test && make -s setup

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
	mcopy -i $(BINDIR)/$(OSNAME).img $(BINDIR)/programs/test/test.elf ::/PROGRAMS

all:
	@echo !==== BOOTLOADER ====!
	@cd src/bootloader && make -s all

	@echo !==== KERNEL ====!
	@cd src/kernel && make -s all

	@echo !==== LIBC ====!
	@cd src/libc && make -s all

	@echo !==== TEST PROGRAM ====!
	@cd src/programs/test && make -s all

	@echo !==== BUILDIMG ====!
	make -s buildimg

clean:	
	@rm -rf $(OBJDIR)
	@rm -rf $(BINDIR)
	
	@echo !==== GNU-EFI
	@cd vendor/gnu-efi && make clean

	@echo !==== BOOTLOADER ====!
	@cd src/bootloader && make -s clean

	@echo !==== KERNEL ====!
	@cd src/kernel && make -s clean

	@echo !==== LIBC ====!
	@cd src/libc && make -s clean

	@echo !==== TEST PROGRAM ====!
	@cd src/programs/test && make -s clean

run:
	qemu-system-x86_64 -drive file=$(BINDIR)/$(OSNAME).img -m 1G -cpu qemu64 -drive if=pflash,format=raw,unit=0,file="$(OVMFDIR)/OVMF_CODE-pure-efi.fd",readonly=on -drive if=pflash,format=raw,unit=1,file="$(OVMFDIR)/OVMF_VARS-pure-efi.fd" -net none
