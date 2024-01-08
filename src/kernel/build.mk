KERNEL_SRC_DIR = $(SRC_DIR)/kernel
KERNEL_BIN_DIR = $(BIN_DIR)/kernel
KERNEL_BUILD_DIR = $(BUILD_DIR)/kernel

KERNEL_OUTPUT = $(KERNEL_BIN_DIR)/kernel.elf

KERNEL_SOURCE = $(call recursive_wildcard, $(KERNEL_SRC_DIR), *.c)
KERNEL_SOURCE += $(call recursive_wildcard, $(KERNEL_SRC_DIR), *.asm)

KERNEL_OBJECTS = $(patsubst $(KERNEL_SRC_DIR)/%, $(KERNEL_BUILD_DIR)/%.o, $(KERNEL_SOURCE))

KERNEL_C_FLAGS = $(C_FLAGS) -mcmodel=large -Wno-array-bounds -I $(KERNEL_SRC_DIR)
KERNE_LD_FLAGS = $(LD_FLAGS) -T $(KERNEL_SRC_DIR)/linker.ld

$(KERNEL_BUILD_DIR)/%.c.o: $(KERNEL_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(KERNEL_C_FLAGS) -c -o $@ $<)

$(KERNEL_BUILD_DIR)/%.asm.o: $(KERNEL_SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(KERNEL_OUTPUT): $(KERNEL_OBJECTS)	
	@echo "!====== BUILDING KERNEL ======!"
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(KERNE_LD_FLAGS) -o $@ $^)

BUILD += $(KERNEL_OUTPUT)