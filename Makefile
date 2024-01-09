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

recursive_wildcard = $(foreach d,$(wildcard $(1:=/*)),$(call recursive_wildcard,$d,$2) $(filter $(subst *,%,$2),$d))

SRC_DIR = src
BIN_DIR = bin
BUILD_DIR = build
ROOT_DIR = root

CC = gcc
LD = ld
ASM = nasm

C_FLAGS = -Os -Wall -ffreestanding -fno-stack-protector -fno-exceptions
LD_FLAGS = -Bsymbolic -nostdlib
ASM_FLAGS = -f elf64

PROGRAM_C_FLAGS = $(C_FLAGS)
PROGRAM_LD_FLAGS = $(LD_FLAGS) bin/libc/libc.o

BUILD = 

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