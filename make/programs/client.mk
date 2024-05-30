CLIENT_SRC_DIR = $(SRC_DIR)/programs/client
CLIENT_BUILD_DIR = $(BUILD_DIR)/programs/client

CLIENT_OUT = $(PROGRAMS_BIN_DIR)/client.elf

CLIENT_SRC = $(wildcard $(CLIENT_SRC_DIR)/*.c)

CLIENT_OBJ = $(patsubst $(SRC_DIR)/%, $(CLIENT_BUILD_DIR)/%.o, $(CLIENT_SRC))

$(CLIENT_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -I$(CLIENT_SRC_DIR) -c -o $@ $<

$(CLIENT_BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(USER_ASM_FLAGS) $^ -o $@

$(CLIENT_OUT): $(CLIENT_OBJ)	
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(CLIENT_OUT)