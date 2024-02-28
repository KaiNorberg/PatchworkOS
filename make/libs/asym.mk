ASYM_LIB_SRC_DIR = $(LIB_SRC_DIR)/asym
ASYM_LIB_BUILD_DIR = $(LIB_BUILD_DIR)/asym

ASYM_LIB_OUTPUT = $(LIB_BIN_DIR)/libasym.a

ASYM_LIB_OBJECTS = $(call objects_pathsubst,$(ASYM_LIB_SRC_DIR),$(ASYM_LIB_BUILD_DIR),.c)
ASYM_LIB_OBJECTS += $(call objects_pathsubst,$(ASYM_LIB_SRC_DIR),$(ASYM_LIB_BUILD_DIR),.s)

$(ASYM_LIB_BUILD_DIR)/%.c.o: $(ASYM_LIB_SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(LIB_C_FLAGS) -I $(ASYM_LIB_SRC_DIR) -c -o $@ $<

$(ASYM_LIB_BUILD_DIR)/%.s.o: $(ASYM_LIB_SRC_DIR)/%.s
	@mkdir -p $(@D)
	$(ASM) $(ASM_FLAGS) $^ -o $@

$(ASYM_LIB_OUTPUT): $(ASYM_LIB_OBJECTS)	
	@mkdir -p $(@D)
	$(LD) $(LD_FLAGS) -r -o $@ $^

BUILD += $(ASYM_LIB_OUTPUT)