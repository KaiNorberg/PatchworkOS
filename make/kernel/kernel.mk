KERNEL_SRC_DIR = $(SRC_DIR)/kernel
KERNEL_BIN_DIR = $(BIN_DIR)/kernel
KERNEL_BUILD_DIR = $(BUILD_DIR)/kernel

KERNEL_OUT = $(KERNEL_BIN_DIR)/kernel.elf

KERNEL_SRC = \
	$(call recursive_wildcard, $(KERNEL_SRC_DIR), *.c) \
	$(call recursive_wildcard, $(KERNEL_SRC_DIR), *.s) \
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
	$(STDLIB)/string/strcat.c

KERNEL_OBJ = $(patsubst $(SRC_DIR)/%, $(KERNEL_BUILD_DIR)/%.o, $(KERNEL_SRC))

KERNEL_C_FLAGS = $(BASE_C_FLAGS) \
	-ffreestanding \
	-fno-stack-protector -fno-exceptions \
	-fno-pie -mcmodel=kernel \
	-mno-red-zone -Wno-array-bounds \
	-D__KERNEL__ \
	-I$(KERNEL_SRC_DIR)

$(KERNEL_BUILD_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(KERNEL_C_FLAGS) -c -o $@ $<

$(KERNEL_BUILD_DIR)/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(BASE_ASM_FLAGS) -I$(KERNEL_SRC_DIR) $^ -o $@

$(KERNEL_OUT): $(KERNEL_OBJ)	
	$(MKCWD)
	$(LD) $(BASE_LD_FLAGS) -T$(KERNEL_SRC_DIR)/linker.ld -o $@ $^

BUILD += $(KERNEL_OUT)