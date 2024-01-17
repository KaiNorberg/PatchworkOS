TEST_SRC_DIR = $(SRC_DIR)/programs/test
TEST_BUILD_DIR = $(BUILD_DIR)/programs/test

TEST_OUTPUT = $(PROGRAMS_BIN_DIR)/test.elf

TEST_OBJECTS = $(call objects_pathsubst,$(TEST_SRC_DIR),$(TEST_BUILD_DIR),.c)
TEST_OBJECTS += $(call objects_pathsubst,$(TEST_SRC_DIR),$(TEST_BUILD_DIR),.asm)

$(TEST_BUILD_DIR)/%.c.o: $(TEST_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(PROGRAM_C_FLAGS) -I $(TEST_SRC_DIR) -c -o $@ $<)

$(TEST_BUILD_DIR)/%.asm.o: $(TEST_SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(TEST_OUTPUT): $(TEST_OBJECTS)
	@echo "!====== BUILDING TEST ======!"
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(PROGRAM_LD_FLAGS) -lpatch-process -o $@ $^)

BUILD += $(TEST_OUTPUT)