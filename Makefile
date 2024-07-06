include Make.defaults

MODULES := $(basename $(notdir $(wildcard make/*.mk)))
PROGRAMS := $(basename $(notdir $(wildcard make/programs/*.mk)))
TARGET := bin/PatchworkOS.img

setup:
	make -C lib/gnu-efi all

build:
	@for MODULE in $(MODULES) ; do \
	   $(MAKE) -f make/$$MODULE.mk SRCDIR=src/$$MODULE BUILDDIR=build/$$MODULE BINDIR=bin/$$MODULE ; \
	done
	@for PROGRAM in $(PROGRAMS) ; do \
	   $(MAKE) -f make/programs/$$PROGRAM.mk SRCDIR=src/programs/$$PROGRAM BUILDDIR=build/programs/$$PROGRAM BINDIR=bin/programs ; \
	done

deploy:
	@echo "!====== RUNNING DEPLOY ======!"
	dd if=/dev/zero of=$(TARGET) bs=1M count=64
	mkfs.vfat -F 32 -n "PATCHWORKOS" $(TARGET)
	mlabel -i $(TARGET) ::PatchworkOS
	mmd -i $(TARGET) ::/boot
	mmd -i $(TARGET) ::/efi
	mmd -i $(TARGET) ::/efi/boot
	mmd -i $(TARGET) ::/usr
	mmd -i $(TARGET) ::/usr/licence
	mcopy -i $(TARGET) -s root/* ::
	mcopy -i $(TARGET) -s bin/bootloader/bootx64.efi ::/efi/boot
	mcopy -i $(TARGET) -s bin/kernel/kernel.elf ::/boot
	mcopy -i $(TARGET) -s bin/programs ::/bin
	mcopy -i $(TARGET) -s COPYING ::/usr/licence
	mcopy -i $(TARGET) -s LICENSE ::/usr/licence

all: build deploy

compile_commands:
	bear -- make build

format:
	find src/ include/ -iname '*.h' -o -iname '*.c' | xargs clang-format -style=file -i

run:
	@qemu-system-x86_64 \
	-M q35 \
	-display sdl \
	-drive file=$(TARGET) \
	-m 1G \
	-smp 6 \
	-serial stdio \
	-no-shutdown -no-reboot \
	-drive if=pflash,format=raw,unit=0,file=lib/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
	-drive if=pflash,format=raw,unit=1,file=lib/OVMFbin/OVMF_VARS-pure-efi.fd \
	-net none

run_debug:
	@qemu-system-x86_64 \
	-M q35 \
	-display sdl \
	-drive file=$(TARGET) \
	-m 1G \
	-smp 6 \
	-serial stdio \
	-d int \
	-no-shutdown -no-reboot \
	-drive if=pflash,format=raw,unit=0,file=lib/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
	-drive if=pflash,format=raw,unit=1,file=lib/OVMFbin/OVMF_VARS-pure-efi.fd \
	-net none

clean:
	rm -rf build
	rm -rf bin

.PHONY: all build deploy
