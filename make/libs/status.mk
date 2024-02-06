STATUS_SRC_DIR = $(LIB_SRC_DIR)/status
STATUS_BUILD_DIR = $(LIB_BUILD_DIR)/status

STATUS_OUTPUT = $(LIB_BIN_DIR)/libstatus.a

STATUS_OBJECTS = $(call objects_pathsubst,$(STATUS_SRC_DIR),$(STATUS_BUILD_DIR),.c)
STATUS_OBJECTS += $(call objects_pathsubst,$(STATUS_SRC_DIR),$(STATUS_BUILD_DIR),.s)

$(STATUS_BUILD_DIR)/%.c.o: $(STATUS_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(LIB_C_FLAGS) -I $(STATUS_SRC_DIR) -c -o $@ $<)

$(STATUS_BUILD_DIR)/%.s.o: $(STATUS_SRC_DIR)/%.s
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(STATUS_OUTPUT): $(STATUS_OBJECTS)	
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(LD_FLAGS) -r -o $@ $^)

BUILD += $(STATUS_OUTPUT)