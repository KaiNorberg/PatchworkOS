SLEEP_TEST_SRC_DIR = $(SRC_DIR)/programs/sleep_test
SLEEP_TEST_BUILD_DIR = $(BUILD_DIR)/programs/sleep_test

SLEEP_TEST_OUTPUT = $(PROGRAMS_BIN_DIR)/sleep_test.elf

SLEEP_TEST_OBJECTS = $(call objects_pathsubst,$(SLEEP_TEST_SRC_DIR),$(SLEEP_TEST_BUILD_DIR),.c)
SLEEP_TEST_OBJECTS += $(call objects_pathsubst,$(SLEEP_TEST_SRC_DIR),$(SLEEP_TEST_BUILD_DIR),.s)

$(SLEEP_TEST_BUILD_DIR)/%.c.o: $(SLEEP_TEST_SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(PROGRAM_C_FLAGS) -I $(SLEEP_TEST_SRC_DIR) -c -o $@ $<

$(SLEEP_TEST_BUILD_DIR)/%.s.o: $(SLEEP_TEST_SRC_DIR)/%.s
	@mkdir -p $(@D)
	$(ASM) $(ASM_FLAGS) $^ -o $@

$(SLEEP_TEST_OUTPUT): $(SLEEP_TEST_OBJECTS)
	@mkdir -p $(@D)
	$(LD) $(PROGRAM_LD_FLAGS) -lsystem -o $@ $^

BUILD += $(SLEEP_TEST_OUTPUT)