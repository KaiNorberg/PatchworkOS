CALC_OUT = $(BIN_DIR)/programs/calculator.elf

CALC_SRC = $(wildcard $(SRC_DIR)/programs/calculator/*.c)

CALC_OBJ = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/calculator/%.o, $(CALC_SRC))

$(BUILD_DIR)/calculator/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -c -o $@ $<

$(CALC_OUT): $(CALC_OBJ)
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(CALC_OUT)
