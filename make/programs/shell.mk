SHELL_SRC_DIR = $(SRC_DIR)/programs/shell
SHELL_BUILD_DIR = $(BUILD_DIR)/programs/shell

SHELL_OUT = $(PROGRAMS_BIN_DIR)/shell.elf

SHELL_SRC = $(wildcard $(SHELL_SRC_DIR)/*.c)

SHELL_OBJ = $(patsubst $(SRC_DIR)/%, $(SHELL_BUILD_DIR)/%.o, $(SHELL_SRC))

$(SHELL_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -I$(SHELL_SRC_DIR) -c -o $@ $<

$(SHELL_BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(USER_ASM_FLAGS) $^ -o $@

$(SHELL_OUT): $(SHELL_OBJ)
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(SHELL_OUT)
