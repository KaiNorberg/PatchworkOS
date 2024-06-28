KERNEL_OUT = bin/kernel/kernel

KERNEL_SRC = \
	$(wildcard src/kernel/*.c) \
	$(wildcard src/kernel/*.s) \
	$(wildcard src/kernel/*/*.c) \
	$(wildcard src/kernel/*/*.s) \
	$(wildcard src/stdlib/*.c) \
	$(wildcard src/stdlib/*.s)

KERNEL_OBJ = $(patsubst src/%, build/kernel/%.o, $(KERNEL_SRC))

KERNEL_C_FLAGS = $(BASE_C_FLAGS) \
	-fno-pic -mcmodel=large \
	-fno-stack-check \
	-mno-red-zone -Wno-array-bounds \
	-fno-stack-protector \
	-fomit-frame-pointer \
	-mno-mmx -mno-3dnow \
	-mno-80387 -mno-sse \
	-mno-sse2 -mno-sse3 \
	-mno-ssse3 -mno-sse4 \
	-D__EMBED__ \
	-Iinclude/kernel \
	-Isrc/kernel

KERNEL_ASM_FLAGS = $(BASE_ASM_FLAGS) \
	-D__EMBED__ \
	-Iinclude/kernel \
	-Isrc/kernel

build/kernel/%.c.o: src/%.c
	$(MKCWD)
	$(CC) $(KERNEL_C_FLAGS) -c -o $@ $<

build/kernel/%.s.o: src/%.s
	$(MKCWD)
	$(ASM) $(KERNEL_ASM_FLAGS) $^ -o $@

$(KERNEL_OUT): $(KERNEL_OBJ)
	$(MKCWD)
	$(LD) $(BASE_LD_FLAGS) -Tsrc/kernel/linker.ld -o $@ $^

BUILD += $(KERNEL_OUT)
