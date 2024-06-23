SHELL_OUT = $(BIN_DIR)/programs/shell.elf

SHELL_SRC = $(wildcard $(SRC_DIR)/programs/shell/*.c)

SHELL_OBJ = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/shell/%.o, $(SHELL_SRC))

$(BUILD_DIR)/shell/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -c -o $@ $<

$(SHELL_OUT): $(SHELL_OBJ)
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(SHELL_OUT)
