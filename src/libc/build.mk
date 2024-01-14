LIBC_SRC_DIR = $(SRC_DIR)/libc
LIBC_BIN_DIR = $(BIN_DIR)/libc
LIBC_BUILD_DIR = $(BUILD_DIR)/libc

LIBC_OUTPUT = $(LIBC_BIN_DIR)/libc.o

LIBC_SOURCE = $(call recursive_wildcard, $(LIBC_SRC_DIR), *.c)
LIBC_SOURCE += $(call recursive_wildcard, $(LIBC_SRC_DIR), *.asm)

LIBC_OBJECTS = $(patsubst $(LIBC_SRC_DIR)/%, $(LIBC_BUILD_DIR)/%.o, $(LIBC_SOURCE))

$(LIBC_BUILD_DIR)/%.c.o: $(LIBC_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(call run_and_test,$(CC) $(LIB_C_FLAGS) -I $(LIBC_SRC_DIR) -c -o $@ $<)

$(LIBC_BUILD_DIR)/%.asm.o: $(LIBC_SRC_DIR)/%.asm
	@mkdir -p $(@D)
	@$(call run_and_test,$(ASM) $(ASM_FLAGS) $^ -o $@)

$(LIBC_OUTPUT): $(LIBC_OBJECTS)	
	@echo "!====== BUILDING LIBC ======!"
	@mkdir -p $(@D)
	@$(call run_and_test,$(LD) $(LD_FLAGS) -r -o $@ $^)

BUILD += $(LIBC_OUTPUT)