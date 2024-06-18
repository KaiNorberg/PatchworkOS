KERNEL_SRC_DIR = $(SRC_DIR)/kernel
KERNEL_BIN_DIR = $(BIN_DIR)/kernel
KERNEL_BUILD_DIR = $(BUILD_DIR)/kernel

KERNEL_OUT = $(KERNEL_BIN_DIR)/kernel.elf

KERNEL_SRC = \
	$(wildcard $(KERNEL_SRC_DIR)/*.c) \
	$(wildcard $(KERNEL_SRC_DIR)/*.s) \
	$(STDLIB)/stdlib/lltoa.c \
	$(STDLIB)/stdlib/ulltoa.c \
	$(STDLIB)/string/memcpy.c \
	$(STDLIB)/string/memmove.c \
	$(STDLIB)/string/strcpy.c \
	$(STDLIB)/string/memcmp.c \
	$(STDLIB)/string/strcmp.c \
	$(STDLIB)/string/strchr.c \
	$(STDLIB)/string/strrchr.c \
	$(STDLIB)/string/memset.c \
	$(STDLIB)/string/strlen.c \
	$(STDLIB)/string/strerror.c \
	$(STDLIB)/string/strncpy.c \
	$(STDLIB)/string/strcat.c \
	$(STDLIB)/stdlib/malloc.c \
	$(STDLIB)/stdlib/calloc.c \
	$(STDLIB)/stdlib/free.c \
	$(STDLIB)/gfx/gfx.c \
	$(STDLIB)/win/win_default_theme.c \
	$(STDLIB)/../internal/init.c

KERNEL_OBJ = $(patsubst $(SRC_DIR)/%, $(KERNEL_BUILD_DIR)/%.o, $(KERNEL_SRC))

KERNEL_C_FLAGS = $(BASE_C_FLAGS) \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic -mcmodel=large \
	-fno-stack-protector \
	-mno-mmx -mno-3dnow \
	-mno-80387 -mno-sse \
	-mno-sse2 -mno-sse3 \
	-mno-ssse3 -mno-sse4 \
	-D__KERNEL__ \
	-I$(KERNEL_SRC_DIR)

$(KERNEL_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(KERNEL_C_FLAGS) -c -o $@ $<

$(KERNEL_BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(BASE_ASM_FLAGS) $^ -o $@

$(KERNEL_OUT): $(KERNEL_OBJ)
	$(MKCWD)
	$(LD) $(BASE_LD_FLAGS) -T$(KERNEL_SRC_DIR)/linker.ld -o $@ $^

BUILD += $(KERNEL_OUT)
