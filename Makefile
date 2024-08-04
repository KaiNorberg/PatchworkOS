include Make.defaults

MODULES := $(basename $(notdir $(wildcard make/*.mk)))
PROGRAMS := $(basename $(notdir $(wildcard make/programs/*.mk)))
TARGET := bin/PatchworkOS.img

setup:
	$(MAKE) -C lib/gnu-efi

$(MODULES): setup
	$(MAKE) -f make/$@.mk SRCDIR=src/$@ BUILDDIR=build/$@ BINDIR=bin/$@

$(PROGRAMS): $(MODULES)
	$(MAKE) -f make/programs/$@.mk SRCDIR=src/programs/$@ BUILDDIR=build/programs/$@ BINDIR=bin/programs

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
	mmd -i $(TARGET) ::/usr/licence
	mcopy -i $(TARGET) -s root/* ::
	mcopy -i $(TARGET) -s bin/bootloader/bootx64.efi ::/efi/boot
	mcopy -i $(TARGET) -s bin/kernel/kernel.elf ::/boot
	mcopy -i $(TARGET) -s bin/programs/shell.elf ::/bin
	mcopy -i $(TARGET) -s bin/programs/calculator.elf ::/usr/bin
	mcopy -i $(TARGET) -s COPYING ::/usr/licence
	mcopy -i $(TARGET) -s LICENSE ::/usr/licence

clean:
	rm -rf build
	rm -rf bin

nuke: clean
	$(MAKE) -C lib/gnu-efi clean

.PHONY: all
all: $(LIBS) $(MODULES) $(PROGRAMS) deploy

compile_commands: clean
	bear -- make all

format:
	find src/ include/ -iname '*.h' -o -iname '*.c' | xargs clang-format -style=file -i

run: all
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

run_debug: all
	@qemu-system-x86_64 \
	-M q35 \
	-display sdl \
	-drive file=$(TARGET) \
	-m 1G \
	-smp 1 \
	-serial stdio \
	-d int \
	-no-shutdown -no-reboot \
	-drive if=pflash,format=raw,unit=0,file=lib/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
	-drive if=pflash,format=raw,unit=1,file=lib/OVMFbin/OVMF_VARS-pure-efi.fd \
	-net none
