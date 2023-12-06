PATCHWORK_DIR := ../..
OUTPUTNAME := bootx64
PARENT_DIR := bootx64

include $(PATCHWORK_DIR)/Make.defaults
include $(PATCHWORK_DIR)/Make.rules

GNU_EFI = $(PATCHWORK_DIR)/vendor/gnu-efi

LD_FLAGS = -Bsymbolic -nostdlib
C_FLAGS = -Os -fpic -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -I$(GNU_EFI)/inc -Wall -Werror

build: $(OBJECTS)

$(BUILD_DIR)/%.asm.o: $(SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test, $(ASM) $(ASM_FLAGS) $^ -o $@, "Compiling")

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test, $(CC) $(C_FLAGS) -c -o $@ $<, "Compiling")

link:
	$(LD) -shared -Bsymbolic -L$(GNU_EFI)/x86_64/lib -L$(GNU_EFI)/x86_64/gnuefi -T$(GNU_EFI)/gnuefi/elf_x86_64_efi.lds $(GNU_EFI)/x86_64/gnuefi/crt0-efi-x86_64.o $(OBJS) -o $(BIN_DIR)/$(OUTPUTNAME).so -lgnuefi -lefi

	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BIN_DIR)/$(OUTPUTNAME).so $(BIN_DIR)/$(OUTPUTNAME).efi

all: 
	@make -s build
	@make -s link

setup:
	$(call run_and_test, mkdir -p $(SRC_DIR) $(BUILD_DIR) $(BIN_DIR), "Running")
	
clean:		
	$(call run_and_test, rm -rf $(BUILD_DIR) $(BIN_DIR), "Running")
	
.PHONY: build link all setup clean