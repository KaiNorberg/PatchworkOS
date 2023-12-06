PATCHWORK_DIR := ../..
OUTPUTNAME := kernel.elf
PARENT_DIR := kernel

include $(PATCHWORK_DIR)/Make.defaults
include $(PATCHWORK_DIR)/Make.rules

build: $(OBJECTS)

$(BUILD_DIR)/idt/interrupts/interrupts.o: $(SRC_DIR)/idt/interrupts/interrupts.c
	@mkdir -p $(@D)
	@$(call run_and_test, $(CC) $(C_FLAGS) -mno-red-zone -mgeneral-regs-only -c -o $@ $<, "Compiling")

$(BUILD_DIR)/syscall/syscall.o: $(SRC_DIR)/syscall/syscall.c
	@mkdir -p $(@D)
	@$(call run_and_test, $(CC) $(C_FLAGS) -mno-red-zone -mgeneral-regs-only -c -o $@ $<, "Compiling")

$(BUILD_DIR)/%.asm.o: $(SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test, $(ASM) $(ASM_FLAGS) $^ -o $@, "Compiling")

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test, $(CC) $(C_FLAGS) -c -o $@ $<, "Compiling")

link: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	@$(call run_and_test, $(LD) $(LD_FLAGS) -o $@ $^, "Linking")

all: 
	@make -s build
	@make -s link

setup:
	$(call run_and_test, mkdir -p $(SRC_DIR) $(BUILD_DIR) $(BIN_DIR), "Running")

clean:		
	$(call run_and_test, rm -rf $(BUILD_DIR) $(BIN_DIR), "Running")

.PHONY: build link all setup clean