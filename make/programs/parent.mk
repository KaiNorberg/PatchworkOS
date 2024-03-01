PARENT_SRC_DIR = $(SRC_DIR)/programs/parent
PARENT_BUILD_DIR = $(BUILD_DIR)/programs/parent

PARENT_OUTPUT = $(PROGRAMS_BIN_DIR)/parent.elf

PARENT_OBJECTS = $(call objects_pathsubst,$(PARENT_SRC_DIR),$(PARENT_BUILD_DIR),.c)
PARENT_OBJECTS += $(call objects_pathsubst,$(PARENT_SRC_DIR),$(PARENT_BUILD_DIR),.s)

$(PARENT_BUILD_DIR)/%.c.o: $(PARENT_SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(PROGRAM_C_FLAGS) -I $(PARENT_SRC_DIR) -c -o $@ $<

$(PARENT_BUILD_DIR)/%.s.o: $(PARENT_SRC_DIR)/%.s
	@mkdir -p $(@D)
	$(ASM) $(ASM_FLAGS) $^ -o $@

$(PARENT_OUTPUT): $(PARENT_OBJECTS)	
	@mkdir -p $(@D)
	$(LD) $(PROGRAM_LD_FLAGS) -lsystem -o $@ $^

BUILD += $(PARENT_OUTPUT)