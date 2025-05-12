include Make.defaults

MODULES := $(basename $(notdir $(wildcard make/*.mk)))
PROGRAMS := $(basename $(notdir $(wildcard make/programs/*.mk)))
TARGET := bin/PatchworkOS.img

ROOT_PROGRAMS := init wall cursor taskbar startmenu dwm

setup:
	$(MAKE) -C lib/gnu-efi

$(MODULES): setup
	$(MAKE) -f make/$@.mk SRCDIR=src/$@ BUILDDIR=build/$@ BINDIR=bin/$@

$(PROGRAMS): $(MODULES)
	$(MAKE) -f make/programs/$@.mk SRCDIR=src/programs/$@ BUILDDIR=build/programs/$@ BINDIR=bin/programs PROGRAM=$@

deploy: $(PROGRAMS)
	dd if=/dev/zero of=$(TARGET) bs=1M count=64
	mkfs.vfat -F 32 -n "PATCHWORKOS" $(TARGET)
	mlabel -i $(TARGET) ::PatchworkOS
	mmd -i $(TARGET) ::/boot
	mmd -i $(TARGET) ::/bin
	mmd -i $(TARGET) ::/efi
	mmd -i $(TARGET) ::/efi/boot
	mmd -i $(TARGET) ::/usr
	mmd -i $(TARGET) ::/usr/bin
	mmd -i $(TARGET) ::/usr/license
	mcopy -i $(TARGET) -s root/* ::
	mcopy -i $(TARGET) -s bin/bootloader/bootx64.efi ::/efi/boot
	mcopy -i $(TARGET) -s bin/kernel/kernel ::/boot
	$(foreach prog,$(ROOT_PROGRAMS),mcopy -i $(TARGET) -s bin/programs/$(prog) ::/bin;)
	$(foreach prog,$(filter-out $(ROOT_PROGRAMS),$(PROGRAMS)),mcopy -i $(TARGET) -s bin/programs/$(prog) ::/usr/bin;)
	mcopy -i $(TARGET) -s LICENSE ::/usr/license

clean:
	rm -rf build
	rm -rf bin

nuke: clean
	$(MAKE) -C lib/gnu-efi clean

.PHONY: all
all: setup $(MODULES) $(PROGRAMS) deploy

compile_commands: clean
	bear -- make all

format:
	find src/ include/ -iname '*.h' -o -iname '*.c' | xargs clang-format -style=file -i

run:
	@qemu-system-x86_64 \
	-M q35 \
	-display sdl \
	-drive file=$(TARGET) \
	-m 1G \
	-smp 1 \
	-serial stdio \
	-no-shutdown -no-reboot \
	-drive if=pflash,format=raw,unit=0,file=lib/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
	-drive if=pflash,format=raw,unit=1,file=lib/OVMFbin/OVMF_VARS-pure-efi.fd \
	-net none

run_debug:
	@qemu-system-x86_64 \
	-M q35 \
	-s \
	-display sdl \
	-drive file=$(TARGET) \
	-m 1G \
	-smp 8 \
	-d int \
	-serial stdio \
	-no-shutdown -no-reboot \
	-drive if=pflash,format=raw,unit=0,file=lib/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
	-drive if=pflash,format=raw,unit=1,file=lib/OVMFbin/OVMF_VARS-pure-efi.fd \
	-net none
