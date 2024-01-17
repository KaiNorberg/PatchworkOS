KERNEL_SRC_DIR = $(SRC_DIR)/kernel
KERNEL_BIN_DIR = $(BIN_DIR)/kernel
KERNEL_BUILD_DIR = $(BUILD_DIR)/kernel

KERNEL_OUTPUT = $(KERNEL_BIN_DIR)/kernel.elf

KERNEL_OBJECTS = $(call objects_pathsubst,$(KERNEL_SRC_DIR),$(KERNEL_BUILD_DIR),.c)
KERNEL_OBJECTS += $(call objects_pathsubst,$(KERNEL_SRC_DIR),$(KERNEL_BUILD_DIR),.asm)

$(KERNEL_BUILD_DIR)/%.c.o: $(KERNEL_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(KERNEL_C_FLAGS) -I $(KERNEL_SRC_DIR) -c -o $@ $<)

$(KERNEL_BUILD_DIR)/%.asm.o: $(KERNEL_SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(KERNEL_OUTPUT): $(KERNEL_OBJECTS)	
	@echo "!====== BUILDING KERNEL ======!"
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(LD_FLAGS) -T $(KERNEL_SRC_DIR)/linker.ld -o $@ $^)

BUILD += $(KERNEL_OUTPUT)