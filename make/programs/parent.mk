PARENT_SRC_DIR = $(SRC_DIR)/programs/parent
PARENT_BUILD_DIR = $(BUILD_DIR)/programs/parent

PARENT_OUT = $(PROGRAMS_BIN_DIR)/parent.elf

PARENT_SRC = $(wildcard $(PARENT_SRC_DIR)/*.c)

PARENT_OBJ = $(patsubst $(SRC_DIR)/%, $(PARENT_BUILD_DIR)/%.o, $(PARENT_SRC))

$(PARENT_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -I $(PARENT_SRC_DIR) -c -o $@ $<

$(PARENT_BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(USER_ASM_FLAGS) $^ -o $@

$(PARENT_OUT): $(PARENT_OBJ)	
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(PARENT_OUT)