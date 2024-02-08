LIBC_SRC_DIR = $(LIB_SRC_DIR)/libc
LIBC_BUILD_DIR = $(LIB_BUILD_DIR)/libc

LIBC_OUTPUT = $(LIB_BIN_DIR)/libstdlib.a

LIBC_OBJECTS = $(call objects_pathsubst,$(LIBC_SRC_DIR),$(LIBC_BUILD_DIR),.c)
LIBC_OBJECTS += $(call objects_pathsubst,$(LIBC_SRC_DIR),$(LIBC_BUILD_DIR),.s)

$(LIBC_BUILD_DIR)/%.c.o: $(LIBC_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(LIB_C_FLAGS) -I $(LIBC_SRC_DIR) -c -o $@ $<)

$(LIBC_BUILD_DIR)/%.s.o: $(LIBC_SRC_DIR)/%.s
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(LIBC_OUTPUT): $(LIBC_OBJECTS)	
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(LD_FLAGS) -r -o $@ $^)

BUILD += $(LIBC_OUTPUT)