PATCHWORK_DIR := ../..
OUTPUTNAME := libc.o
PARENT_DIR := libc

include $(PATCHWORK_DIR)/Make.defaults
include $(PATCHWORK_DIR)/Make.rules

LD_FLAGS = -T $(LINKER_SCRIPT) -Bstatic -nostdlib -r

build: $(OBJECTS)

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