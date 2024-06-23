STD_OUT = $(BIN_DIR)/stdlib/libstd.a

STD_SRC = \
	$(wildcard $(SRC_DIR)/stdlib/*.c) \
	$(wildcard $(SRC_DIR)/stdlib/*.s)

STD_OBJ = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/stdlib/%.o, $(STD_SRC))

STD_C_FLAGS = $(BASE_C_FLAGS) \
	-I$(INCLUDE_DIR)/stdlib \
	-I$(INCLUDE_DIR)/stdlib_internal

STD_ASM_FLAGS = $(BASE_ASM_FLAGS) \

$(BUILD_DIR)/stdlib/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(STD_C_FLAGS) -c -o $@ $<

$(BUILD_DIR)/stdlib/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(STD_ASM_FLAGS) $^ -o $@

$(STD_OUT): $(STD_OBJ)
	$(MKCWD)
	$(LD) $(BASE_LD_FLAGS) -r -o $@ $^

BUILD += $(STD_OUT)
