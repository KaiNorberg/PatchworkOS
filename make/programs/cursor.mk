CURSOR_OUT = bin/programs/cursor.elf

CURSOR_SRC = $(wildcard src/programs/cursor/*.c)

CURSOR_OBJ = $(patsubst src/%, build/cursor/%.o, $(CURSOR_SRC))

build/cursor/%.c.o: src/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -c -o $@ $<

$(CURSOR_OUT): $(CURSOR_OBJ)
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(CURSOR_OUT)
