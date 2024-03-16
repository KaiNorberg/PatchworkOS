# arg1 = dir
# arg2 = pattern
recursive_wildcard = \
	$(foreach d,$(wildcard $(1:=/*)),$(call recursive_wildcard,$d,$2) $(filter $(subst *,%,$2),$d))

# arg1 = src dir
# arg2 = build dir
# arg3 = extension
objects_pathsubst = \
	$(patsubst $(1)/%$(3), $(2)/%$(3).o, $(call recursive_wildcard, $(1), *$(3)))

SRC_DIR = src
BIN_DIR = bin
BUILD_DIR = build
MAKE_DIR = make

OUTPUT_IMAGE = bin/PatchworkOS.img

LIB_SRC_DIR = src/libs
LIB_BIN_DIR = bin/libs
LIB_BUILD_DIR = build/libs
LIBC_FUNCTIONS = $(LIB_SRC_DIR)/libc/functions

PROGRAMS_BIN_DIR = bin/programs

ROOT_DIR = root

CC = gcc
LD = ld
ASM = nasm

ASM_FLAGS = -f elf64 \
	-I$(LIB_SRC_DIR) \
	-I$(LIB_SRC_DIR)/include \

BASE_C_FLAGS = -O3 \
	-Wall \
	-Wextra \
	-Werror \
	-Wshadow \
	-Wno-unused-variable \
	-Wno-ignored-qualifiers \
	-Wno-unused-parameter \
	-Wno-unused-but-set-variable \
	-Wno-implicit-fallthrough \
	-mno-80387 -mno-mmx -mno-3dnow \
	-mno-sse -mno-sse2 \
	-I$(SRC_DIR) \
	-I$(LIB_SRC_DIR)/include
	
KERNEL_C_FLAGS = $(BASE_C_FLAGS) \
	-ffreestanding \
	-fno-stack-protector -fno-exceptions \
	-fno-pie -mcmodel=kernel \
	-mno-red-zone -Wno-array-bounds \
	-D__KERNEL__

BOOT_C_FLAGS = $(BASE_C_FLAGS) \
	-fpic -ffreestanding \
	-fno-stack-protector -fno-stack-check \
	-fshort-wchar -mno-red-zone -Wno-array-bounds \
	-mno-80387 -mno-mmx -mno-3dnow \
	-mno-sse -mno-sse2 \
	-D__BOOTLOADER__

LIB_C_FLAGS = $(BASE_C_FLAGS) \
	-ffreestanding

PROGRAM_C_FLAGS = $(BASE_C_FLAGS) \
	-ffreestanding

LD_FLAGS = -nostdlib

PROGRAM_LD_FLAGS = $(LD_FLAGS) \
	-L$(LIB_BIN_DIR) -lcrt

include $(call recursive_wildcard, $(MAKE_DIR), *.mk)

setup:
	@echo "!====== RUNNING SETUP  ======!"
	@cd vendor/gnu-efi && make all && cd ../..

build: $(BUILD)

deploy:
	@echo "!====== RUNNING DEPLOY ======!"
	dd status=progress if=/dev/zero of=$(OUTPUT_IMAGE) bs=4096 count=1024
	mkfs -t vfat $(OUTPUT_IMAGE)
	mlabel -i $(OUTPUT_IMAGE) -s ::PatchworkOS
	mmd -i $(OUTPUT_IMAGE) ::/boot
	mmd -i $(OUTPUT_IMAGE) ::/efi
	mmd -i $(OUTPUT_IMAGE) ::/efi/boot
	mcopy -i $(OUTPUT_IMAGE) -s $(BOOT_OUTPUT_EFI) ::efi/boot
	mcopy -i $(OUTPUT_IMAGE) -s $(KERNEL_OUTPUT) ::boot
	mcopy -i $(OUTPUT_IMAGE) -s $(ROOT_DIR)/* ::
	mcopy -i $(OUTPUT_IMAGE) -s $(BIN_DIR)/programs ::/bin

all: build deploy

run:
	@qemu-system-x86_64 \
    -drive file=$(OUTPUT_IMAGE) \
    -m 1G \
	-smp 4 \
    -no-shutdown -no-reboot \
    -drive if=pflash,format=raw,unit=0,file=vendor/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
    -drive if=pflash,format=raw,unit=1,file=vendor/OVMFbin/OVMF_VARS-pure-efi.fd \
    -net none

run-debug:
	@qemu-system-x86_64 \
    -drive file=$(OUTPUT_IMAGE) \
	-m 1G \
	-smp 4 \
    -serial stdio \
	-d int \
    -no-shutdown -no-reboot \
    -drive if=pflash,format=raw,unit=0,file=vendor/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
    -drive if=pflash,format=raw,unit=1,file=vendor/OVMFbin/OVMF_VARS-pure-efi.fd \
    -net none

clean:		
#@cd vendor/gnu-efi && make clean && cd ../..
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)

.PHONY: build clean