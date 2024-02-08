SLEEP_TEST_SRC_DIR = $(SRC_DIR)/programs/sleep_test
SLEEP_TEST_BUILD_DIR = $(BUILD_DIR)/programs/sleep_test

SLEEP_TEST_OUTPUT = $(PROGRAMS_BIN_DIR)/sleep_test.elf

SLEEP_TEST_OBJECTS = $(call objects_pathsubst,$(SLEEP_TEST_SRC_DIR),$(SLEEP_TEST_BUILD_DIR),.c)
SLEEP_TEST_OBJECTS += $(call objects_pathsubst,$(SLEEP_TEST_SRC_DIR),$(SLEEP_TEST_BUILD_DIR),.s)

$(SLEEP_TEST_BUILD_DIR)/%.c.o: $(SLEEP_TEST_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(PROGRAM_C_FLAGS) -I $(SLEEP_TEST_SRC_DIR) -c -o $@ $<)

$(SLEEP_TEST_BUILD_DIR)/%.s.o: $(SLEEP_TEST_SRC_DIR)/%.s
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(SLEEP_TEST_OUTPUT): $(SLEEP_TEST_OBJECTS)
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(PROGRAM_LD_FLAGS) -lasym -o $@ $^)

BUILD += $(SLEEP_TEST_OUTPUT)