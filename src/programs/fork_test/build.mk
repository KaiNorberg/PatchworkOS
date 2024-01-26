FORK_TEST_SRC_DIR = $(SRC_DIR)/programs/fork_test
FORK_TEST_BUILD_DIR = $(BUILD_DIR)/programs/fork_test

FORK_TEST_OUTPUT = $(PROGRAMS_BIN_DIR)/fork_test.elf

FORK_TEST_OBJECTS = $(call objects_pathsubst,$(FORK_TEST_SRC_DIR),$(FORK_TEST_BUILD_DIR),.c)
FORK_TEST_OBJECTS += $(call objects_pathsubst,$(FORK_TEST_SRC_DIR),$(FORK_TEST_BUILD_DIR),.s)

$(FORK_TEST_BUILD_DIR)/%.c.o: $(FORK_TEST_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(PROGRAM_C_FLAGS) -I $(FORK_TEST_SRC_DIR) -c -o $@ $<)

$(FORK_TEST_BUILD_DIR)/%.s.o: $(FORK_TEST_SRC_DIR)/%.s
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(FORK_TEST_OUTPUT): $(FORK_TEST_OBJECTS)	
	@echo "!====== BUILDING FORK_TEST ======!"
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(PROGRAM_LD_FLAGS) -lprocess -o $@ $^)

BUILD += $(FORK_TEST_OUTPUT)