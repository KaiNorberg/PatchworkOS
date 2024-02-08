CRT_SRC_DIR = $(LIB_SRC_DIR)/crt
CRT_BUILD_DIR = $(LIB_BUILD_DIR)/crt

CRT_OUTPUT = $(LIB_BIN_DIR)/libcrt.a

CRT_OBJECTS = $(call objects_pathsubst,$(CRT_SRC_DIR),$(CRT_BUILD_DIR),.c)
CRT_OBJECTS += $(call objects_pathsubst,$(CRT_SRC_DIR),$(CRT_BUILD_DIR),.s)

$(CRT_BUILD_DIR)/%.c.o: $(CRT_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(LIB_C_FLAGS) -I $(CRT_SRC_DIR) -c -o $@ $<)

$(CRT_BUILD_DIR)/%.s.o: $(CRT_SRC_DIR)/%.s
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(CRT_OUTPUT): $(CRT_OBJECTS)	
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(LD_FLAGS) -r -o $@ $^)

BUILD += $(CRT_OUTPUT)