TEST_SRC_DIR = $(SRC_DIR)/programs/test
TEST_BIN_DIR = $(BIN_DIR)/programs
TEST_BUILD_DIR = $(BUILD_DIR)/programs/test

TEST_OUTPUT = $(TEST_BIN_DIR)/test.elf

TEST_SOURCE = $(call recursive_wildcard, $(TEST_SRC_DIR), *.c)
TEST_SOURCE += $(call recursive_wildcard, $(TEST_SRC_DIR), *.asm)

TEST_OBJECTS = $(patsubst $(TEST_SRC_DIR)/%, $(TEST_BUILD_DIR)/%.o, $(TEST_SOURCE))

$(TEST_BUILD_DIR)/%.c.o: $(TEST_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(PROGRAM_C_FLAGS) -I $(TEST_SRC_DIR) -c -o $@ $<)

$(TEST_BUILD_DIR)/%.asm.o: $(TEST_SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(TEST_OUTPUT): $(TEST_OBJECTS)
	@echo "!====== BUILDING TEST ======!"
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(PROGRAM_LD_FLAGS) -o $@ $^)

BUILD += $(TEST_OUTPUT)