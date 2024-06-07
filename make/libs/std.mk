STD_SRC_DIR = $(LIBS_SRC_DIR)/std
STD_BUILD_DIR = $(LIBS_BUILD_DIR)/std

STD_OUT = $(LIBS_BIN_DIR)/libstd.a

STD_SRC = $(wildcard $(STD_SRC_DIR)/functions/*/*.c) \
	$(wildcard $(STD_SRC_DIR)/functions/*/*.s) \
	$(wildcard $(STD_SRC_DIR)/internal/*.c) \
	$(wildcard $(STD_SRC_DIR)/internal/*.s)

STD_OBJ = $(patsubst $(SRC_DIR)/%, $(STD_BUILD_DIR)/%.o, $(STD_SRC))

$(STD_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -I$(STD_SRC_DIR) -c -o $@ $<

$(STD_BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(USER_ASM_FLAGS) $^ -o $@

$(STD_OUT): $(STD_OBJ)	
	$(MKCWD)
	$(LD) $(BASE_LD_FLAGS) -r -o $@ $^

BUILD += $(STD_OUT)