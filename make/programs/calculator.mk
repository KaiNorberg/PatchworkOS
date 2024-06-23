CALC_OUT = bin/programs/calculator.elf

CALC_SRC = $(wildcard src/programs/calculator/*.c)

CALC_OBJ = $(patsubst src/%, build/calculator/%.o, $(CALC_SRC))

build/calculator/%.c.o: src/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -c -o $@ $<

$(CALC_OUT): $(CALC_OBJ)
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(CALC_OUT)
