PATCHWORK_DIR := ../../..
OUTPUTNAME := test2.elf
PARENT_DIR := programs/test2

include $(PATCHWORK_DIR)/Make.defaults

LINKER_SCRIPT = ../linker.ld

LD_FLAGS = -T $(LINKER_SCRIPT) -Bstatic -nostdlib ../../../bin/libc/libc.o

build: $(OBJECTS)

$(BUILD_DIR)/%.asm.o: $(SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(C_FLAGS) -c -o $@ $<)

link: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	@$(call run_and_test,$(LD) $(LD_FLAGS) -o $@ $^)

all: 
	@make -s build
	@make -s link

setup:
	$(call run_and_test,mkdir -p $(SRC_DIR) $(BUILD_DIR) $(BIN_DIR))

clean:		
	$(call run_and_test,rm -rf $(BUILD_DIR) $(BIN_DIR))

.PHONY: build link all setup clean