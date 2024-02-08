CHILD_SRC_DIR = $(SRC_DIR)/programs/child
CHILD_BUILD_DIR = $(BUILD_DIR)/programs/child

CHILD_OUTPUT = $(PROGRAMS_BIN_DIR)/child.elf

CHILD_OBJECTS = $(call objects_pathsubst,$(CHILD_SRC_DIR),$(CHILD_BUILD_DIR),.c)
CHILD_OBJECTS += $(call objects_pathsubst,$(CHILD_SRC_DIR),$(CHILD_BUILD_DIR),.s)

$(CHILD_BUILD_DIR)/%.c.o: $(CHILD_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(PROGRAM_C_FLAGS) -I $(CHILD_SRC_DIR) -c -o $@ $<)

$(CHILD_BUILD_DIR)/%.s.o: $(CHILD_SRC_DIR)/%.s
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(CHILD_OUTPUT): $(CHILD_OBJECTS)	
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(PROGRAM_LD_FLAGS) -lc -lprocess -lstatus -lfilesystem -o $@ $^)

BUILD += $(CHILD_OUTPUT)