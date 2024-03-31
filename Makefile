# arg1 = dir
# arg2 = pattern
recursive_wildcard = \
	$(foreach d,$(wildcard $(1:=/*)),$(call recursive_wildcard,$d,$2) $(filter $(subst *,%,$2),$d))

MKCWD = mkdir -p $(@D)

CC = gcc
LD = ld
ASM = nasm

SRC_DIR = src
BIN_DIR = bin
BUILD_DIR = build
MAKE_DIR = make
ROOT_DIR = root

LIBS_SRC_DIR = $(SRC_DIR)/libs
LIBS_BIN_DIR = $(BIN_DIR)/libs
LIBS_BUILD_DIR = $(BUILD_DIR)/libs
STDLIB = $(LIBS_SRC_DIR)/std/functions

PROGRAMS_BIN_DIR = $(BIN_DIR)/programs

OUTPUT_IMAGE = $(BIN_DIR)/PatchworkOS.img

BASE_ASM_FLAGS = -f elf64 \
	-I$(LIBS_SRC_DIR)/std \
	-I$(LIBS_SRC_DIR)/std/include

BASE_C_FLAGS = -O3 \
	-nostdlib \
	-Wall \
	-Wextra \
	-Werror \
	-Wno-unused-variable \
	-Wno-ignored-qualifiers \
	-Wno-unused-parameter \
	-Wno-unused-but-set-variable \
	-Wno-implicit-fallthrough \
	-mno-80387 -mno-mmx -mno-3dnow \
	-mno-sse -mno-sse2 \
	-fno-stack-protector \
	-I$(LIBS_SRC_DIR)/std \
	-I$(LIBS_SRC_DIR)/std/include \
	-I$(SRC_DIR)

BASE_LD_FLAGS = -nostdlib

USER_C_FLAGS = $(BASE_C_FLAGS) \
	-ffreestanding

USER_ASM_FLAGS = $(BASE_ASM_FLAGS)

USER_LD_FLAGS = $(LD_FLAGS) \
	-L$(LIBS_BIN_DIR) -lstd

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
	mcopy -i $(OUTPUT_IMAGE) -s $(BOOT_OUT_EFI) ::efi/boot
	mcopy -i $(OUTPUT_IMAGE) -s $(KERNEL_OUT) ::boot
	mcopy -i $(OUTPUT_IMAGE) -s $(ROOT_DIR)/* ::
	mcopy -i $(OUTPUT_IMAGE) -s $(BIN_DIR)/programs ::/programs

all: build deploy

run:
	@qemu-system-x86_64 \
    -drive file=$(OUTPUT_IMAGE) \
    -m 100M \
	-smp 8 \
    -no-shutdown -no-reboot \
    -drive if=pflash,format=raw,unit=0,file=vendor/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
    -drive if=pflash,format=raw,unit=1,file=vendor/OVMFbin/OVMF_VARS-pure-efi.fd \
    -net none

run-debug:
	@qemu-system-x86_64 \
    -drive file=$(OUTPUT_IMAGE) \
	-m 100M \
	-smp 8 \
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

.PHONY: setup build deploy all run run-debug clean