define run_and_test
printf "\033[m$(1)... "; \
$(1) 2> $@.log; \
RESULT=$$?; \
if [ $$RESULT -ne 0 ]; then \
	printf "\033[0;31mERROR\033[m\n"   ; \
elif [ -s $@.log ]; then \
	printf "\033[0;33mWARNING\033[m\n"   ; \
else  \
	printf "\033[0;32mDONE\033[m\n"   ; \
fi; \
cat $@.log; \
rm -f $@.log; \
exit $$RESULT
endef

# arg1 = dir
# arg2 = extension
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

LIB_SRC_DIR = src/libs
LIB_BIN_DIR = bin/libs
LIB_BUILD_DIR = build/libs

PROGRAMS_BIN_DIR = bin/programs

ROOT_DIR = root

CC = gcc
LD = ld
ASM = nasm

ASM_FLAGS = -f elf64

BASE_C_FLAGS = -Wall \
	-Wextra \
	-Werror \
	-Wshadow \
	-Wno-ignored-qualifiers \
	-Wno-unused-parameter \
	-I$(LIB_SRC_DIR)/include \
	-I$(LIB_SRC_DIR)/libc/include

KERNEL_C_FLAGS = $(BASE_C_FLAGS) \
	-Os -ffreestanding \
	-fno-stack-protector -fno-exceptions \
	-fno-pie -mcmodel=kernel -mno-80387 \
	-mno-mmx -mno-3dnow -mno-sse \
	-mno-sse2 -mno-red-zone -Wno-array-bounds

BOOT_C_FLAGS = $(BASE_C_FLAGS) \
	-fpic -ffreestanding -mno-sse2 \
	-fno-stack-protector -fno-stack-check \
	-fshort-wchar -mno-red-zone \
	-mno-80387 -Wno-array-bounds \
	-mno-mmx -mno-3dnow -mno-sse \
	-I$(BOOT_SRC_DIR) -I$(GNU_EFI)/inc

LIB_C_FLAGS = $(BASE_C_FLAGS) \
	-Os -ffreestanding

PROGRAM_C_FLAGS = $(BASE_C_FLAGS) \
	-Os -ffreestanding

LD_FLAGS = -nostdlib

PROGRAM_LD_FLAGS = $(LD_FLAGS) \
	-L$(LIB_BIN_DIR) -lcrt

include $(call recursive_wildcard, $(SRC_DIR), *.mk)

setup:
	@echo "!====== RUNNING SETUP  ======!"
	@cd vendor/gnu-efi && make all && cd ../..

build: $(BUILD)

deploy:
	@echo "!====== RUNNING DEPLOY ======!"
	@dd if=/dev/zero of=bin/Patchwork.img bs=4096 count=1024
	@$(call run_and_test,mkfs -t vfat bin/Patchwork.img)                           
	@$(call run_and_test,mmd -i bin/Patchwork.img ::/boot)                        
	@$(call run_and_test,mmd -i bin/Patchwork.img ::/efi)                    
	@$(call run_and_test,mmd -i bin/Patchwork.img ::/efi/boot)
	@$(call run_and_test,mcopy -i bin/Patchwork.img -s $(BOOT_OUTPUT_EFI) ::efi/boot)
	@$(call run_and_test,mcopy -i bin/Patchwork.img -s $(KERNEL_OUTPUT) ::boot)
	@$(call run_and_test,mcopy -i bin/Patchwork.img -s $(ROOT_DIR)/* ::)
	@$(call run_and_test,mcopy -i bin/Patchwork.img -s $(BIN_DIR)/programs ::/bin)

all: build deploy

run:
	@qemu-system-x86_64 \
    -drive file=bin/Patchwork.img \
    -m 1G \
    -cpu qemu64 \
    -smp 4 \
    -serial stdio \
    -d int \
    -no-shutdown -no-reboot \
    -drive if=pflash,format=raw,unit=0,file=vendor/OVMFbin/OVMF_CODE-pure-efi.fd,readonly=on \
    -drive if=pflash,format=raw,unit=1,file=vendor/OVMFbin/OVMF_VARS-pure-efi.fd \
    -net none
	
clean:		
	@cd vendor/gnu-efi && make clean && cd ../..
	@$(call run_and_test,rm -rf $(BUILD_DIR))
	@$(call run_and_test,rm -rf $(BIN_DIR))

.PHONY: build clean