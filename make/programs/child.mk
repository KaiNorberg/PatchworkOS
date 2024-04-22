CHILD_SRC_DIR = $(SRC_DIR)/programs/child
CHILD_BUILD_DIR = $(BUILD_DIR)/programs/child

CHILD_OUT = $(PROGRAMS_BIN_DIR)/child.elf

CHILD_SRC = $(wildcard $(CHILD_SRC_DIR)/*.c)

CHILD_OBJ = $(patsubst $(SRC_DIR)/%, $(CHILD_BUILD_DIR)/%.o, $(CHILD_SRC))

$(CHILD_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -I $(CHILD_SRC_DIR) -c -o $@ $<

$(CHILD_BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(USER_ASM_FLAGS) $^ -o $@

$(CHILD_OUT): $(CHILD_OBJ)	
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(CHILD_OUT)