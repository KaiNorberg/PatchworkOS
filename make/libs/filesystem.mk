FILESYSTEM_SRC_DIR = $(LIB_SRC_DIR)/filesystem
FILESYSTEM_BUILD_DIR = $(LIB_BUILD_DIR)/filesystem

FILESYSTEM_OUTPUT = $(LIB_BIN_DIR)/libfilesystem.a

FILESYSTEM_OBJECTS = $(call objects_pathsubst,$(FILESYSTEM_SRC_DIR),$(FILESYSTEM_BUILD_DIR),.c)
FILESYSTEM_OBJECTS += $(call objects_pathsubst,$(FILESYSTEM_SRC_DIR),$(FILESYSTEM_BUILD_DIR),.s)

$(FILESYSTEM_BUILD_DIR)/%.c.o: $(FILESYSTEM_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(LIB_C_FLAGS) -I $(FILESYSTEM_SRC_DIR) -c -o $@ $<)

$(FILESYSTEM_BUILD_DIR)/%.s.o: $(FILESYSTEM_SRC_DIR)/%.s
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(FILESYSTEM_OUTPUT): $(FILESYSTEM_OBJECTS)	
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(LD_FLAGS) -r -o $@ $^)

BUILD += $(FILESYSTEM_OUTPUT)