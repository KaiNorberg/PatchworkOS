STD_OUT = bin/stdlib/libstd.a

STD_SRC = \
	$(wildcard src/stdlib/*.c) \
	$(wildcard src/stdlib/*.s) \
	$(wildcard src/stdlib/*/*.c) \
	$(wildcard src/stdlib/*/*.s)

STD_OBJ = $(patsubst src/%, build/stdlib/%.o, $(STD_SRC))

STD_C_FLAGS = $(BASE_C_FLAGS) \
	-Iinclude/stdlib \
	-Iinclude/stdlib_internal

STD_ASM_FLAGS = $(BASE_ASM_FLAGS) \
	-Isrc/stdlib

build/stdlib/%.c.o: src/%.c
	$(MKCWD)
	$(CC) $(STD_C_FLAGS) -c -o $@ $<

build/stdlib/%.s.o: src/%.s
	$(MKCWD)
	$(ASM) $(STD_ASM_FLAGS) $^ -o $@

$(STD_OUT): $(STD_OBJ)
	$(MKCWD)
	$(LD) $(BASE_LD_FLAGS) -r -o $@ $^

BUILD += $(STD_OUT)
