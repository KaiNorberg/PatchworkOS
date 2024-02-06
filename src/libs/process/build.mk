PROCESS_SRC_DIR = $(LIB_SRC_DIR)/process
PROCESS_BUILD_DIR = $(LIB_BUILD_DIR)/process

PROCESS_OUTPUT = $(LIB_BIN_DIR)/libprocess.a

PROCESS_OBJECTS = $(call objects_pathsubst,$(PROCESS_SRC_DIR),$(PROCESS_BUILD_DIR),.c)
PROCESS_OBJECTS += $(call objects_pathsubst,$(PROCESS_SRC_DIR),$(PROCESS_BUILD_DIR),.s)

$(PROCESS_BUILD_DIR)/%.c.o: $(PROCESS_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(LIB_C_FLAGS) -I $(PROCESS_SRC_DIR) -c -o $@ $<)

$(PROCESS_BUILD_DIR)/%.s.o: $(PROCESS_SRC_DIR)/%.s
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(PROCESS_OUTPUT): $(PROCESS_OBJECTS)	
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(LD_FLAGS) -r -o $@ $^)

BUILD += $(PROCESS_OUTPUT)