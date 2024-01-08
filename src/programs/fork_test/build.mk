FORK_TEST_SRC_DIR = $(SRC_DIR)/programs/fork_test
FORK_TEST_BIN_DIR = $(BIN_DIR)/programs
FORK_TEST_BUILD_DIR = $(BUILD_DIR)/programs/fork_test

FORK_TEST_OUTPUT = $(FORK_TEST_BIN_DIR)/fork_test.elf

FORK_TEST_SOURCE = $(call recursive_wildcard, $(FORK_TEST_SRC_DIR), *.c)
FORK_TEST_SOURCE += $(call recursive_wildcard, $(FORK_TEST_SRC_DIR), *.asm)

FORK_TEST_OBJECTS = $(patsubst $(FORK_TEST_SRC_DIR)/%, $(FORK_TEST_BUILD_DIR)/%.o, $(FORK_TEST_SOURCE))

$(FORK_TEST_BUILD_DIR)/%.c.o: $(FORK_TEST_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(PROGRAM_C_FLAGS) -I $(FORK_TEST_SRC_DIR) -c -o $@ $<)

$(FORK_TEST_BUILD_DIR)/%.asm.o: $(FORK_TEST_SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(FORK_TEST_OUTPUT): $(FORK_TEST_OBJECTS)	
	@echo "!====== BUILDING FORK_TEST ======!"
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(PROGRAM_LD_FLAGS) -o $@ $^)

BUILD += $(FORK_TEST_OUTPUT)