MKCWD = mkdir -p $(@D)

CC = gcc
LD = ld
ASM = nasm

OUTPUT_IMAGE = bin/PatchworkOS.img

BASE_ASM_FLAGS = -f elf64 \
	-Iinclude/stdlib \
	-Iinclude

BASE_C_FLAGS = -O3 \
	-Wall \
	-Wextra \
	-Werror \
	-Wno-deprecated-pragma \
	-Wno-unused-function \
	-Wno-unused-variable \
	-Wno-ignored-qualifiers \
	-Wno-unused-parameter \
	-Wno-unused-but-set-variable \
	-Wno-implicit-fallthrough \
	-Wno-deprecated-non-prototype \
	-fno-stack-protector \
	-ffreestanding -nostdlib \
	-Iinclude/stdlib \
	-Iinclude

BASE_LD_FLAGS = -nostdlib

USER_C_FLAGS = $(BASE_C_FLAGS)

USER_ASM_FLAGS = $(BASE_ASM_FLAGS)

USER_LD_FLAGS = $(BASE_LD_FLAGS) \
	-Lbin/stdlib -lstd

include make/bootloader/bootloader.mk
include make/kernel/kernel.mk
include make/stdlib/stdlib.mk
include $(wildcard make/programs/*.mk)

setup:
	@echo "!====== RUNNING SETUP  ======!"
	@cd lib/gnu-efi && make all && cd ../..

build: $(BUILD)

deploy:
	@echo "!====== RUNNING DEPLOY ======!"
	dd if=/dev/zero of=$(OUTPUT_IMAGE) bs=1M count=64
	mkfs.vfat -F 32 -n "PatchworkOS" $(OUTPUT_IMAGE)
	mlabel -i $(OUTPUT_IMAGE) ::PatchworkOS
	mmd -i $(OUTPUT_IMAGE) ::/boot
	mmd -i $(OUTPUT_IMAGE) ::/efi
	mmd -i $(OUTPUT_IMAGE) ::/efi/boot
	mmd -i $(OUTPUT_IMAGE) ::/usr
	mmd -i $(OUTPUT_IMAGE) ::/usr/licence
	mcopy -i $(OUTPUT_IMAGE) -s root/* ::
	mcopy -i $(OUTPUT_IMAGE) -s $(BOOT_OUT_EFI) ::/efi/boot
	mcopy -i $(OUTPUT_IMAGE) -s $(KERNEL_OUT) ::/boot
	mcopy -i $(OUTPUT_IMAGE) -s bin/programs ::/bin
	mcopy -i $(OUTPUT_IMAGE) -s COPYING ::/usr/licence
	mcopy -i $(OUTPUT_IMAGE) -s LICENSE ::/usr/licence

compile_commands:
	bear -- make build

format:
	find src/ include/ -iname '*.h' -o -iname '*.c' | xargs clang-format -style=file -i

all: build deploy

run:
	@qemu-system-x86_64 \
	-M q35 \
	-display sdl \
	-drive file=$(OUTPUT_IMAGE) \
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
    -drive file=$(OUTPUT_IMAGE) \
	-m 1G \
	-smp 6 \
    -serial stdio \
	-d int \
    -no-shutdown -no-reboot \
    -drive if=pflash,format=raw,unit=0,file=lib/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
    -drive if=pflash,format=raw,unit=1,file=lib/OVMFbin/OVMF_VARS-pure-efi.fd \
    -net none

clean:
#@cd lib/gnu-efi && make clean && cd ../..
	rm -rf build
	rm -rf bin

.PHONY: setup build deploy all run run-debug clean
