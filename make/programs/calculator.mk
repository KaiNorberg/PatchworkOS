CALC_SRC_DIR = $(SRC_DIR)/programs/calculator
CALC_BUILD_DIR = $(BUILD_DIR)/programs/calculator

CALC_OUT = $(PROGRAMS_BIN_DIR)/calculator.elf

CALC_SRC = $(wildcard $(CALC_SRC_DIR)/*.c)

CALC_OBJ = $(patsubst $(SRC_DIR)/%, $(CALC_BUILD_DIR)/%.o, $(CALC_SRC))

$(CALC_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -I$(CALC_SRC_DIR) -c -o $@ $<

$(CALC_BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(USER_ASM_FLAGS) $^ -o $@

$(CALC_OUT): $(CALC_OBJ)
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(CALC_OUT)
