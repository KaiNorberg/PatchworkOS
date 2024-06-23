SHELL_OUT = bin/programs/shell.elf

SHELL_SRC = $(wildcard src/programs/shell/*.c)

SHELL_OBJ = $(patsubst src/%, build/shell/%.o, $(SHELL_SRC))

build/shell/%.c.o: src/%.c
	$(MKCWD)
	$(CC) $(USER_C_FLAGS) -c -o $@ $<

$(SHELL_OUT): $(SHELL_OBJ)
	$(MKCWD)
	$(LD) $(USER_LD_FLAGS) -o $@ $^

BUILD += $(SHELL_OUT)
