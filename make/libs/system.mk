SYSTEM_LIB_SRC_DIR = $(LIB_SRC_DIR)/system
SYSTEM_LIB_BUILD_DIR = $(LIB_BUILD_DIR)/system

SYSTEM_LIB_OUTPUT = $(LIB_BIN_DIR)/libsystem.a

SYSTEM_LIB_OBJECTS = $(call objects_pathsubst,$(SYSTEM_LIB_SRC_DIR),$(SYSTEM_LIB_BUILD_DIR),.c)
SYSTEM_LIB_OBJECTS += $(call objects_pathsubst,$(SYSTEM_LIB_SRC_DIR),$(SYSTEM_LIB_BUILD_DIR),.s)

$(SYSTEM_LIB_BUILD_DIR)/%.c.o: $(SYSTEM_LIB_SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(LIB_C_FLAGS) -I $(SYSTEM_LIB_SRC_DIR) -c -o $@ $<

$(SYSTEM_LIB_BUILD_DIR)/%.s.o: $(SYSTEM_LIB_SRC_DIR)/%.s
	@mkdir -p $(@D)
	$(ASM) $(ASM_FLAGS) $^ -o $@

$(SYSTEM_LIB_OUTPUT): $(SYSTEM_LIB_OBJECTS)	
	@mkdir -p $(@D)
	$(LD) $(LD_FLAGS) -r -o $@ $^

BUILD += $(SYSTEM_LIB_OUTPUT)