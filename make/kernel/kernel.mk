KERNEL_SRC_DIR = $(SRC_DIR)/kernel
KERNEL_BIN_DIR = $(BIN_DIR)/kernel
KERNEL_BUILD_DIR = $(BUILD_DIR)/kernel

KERNEL_OUTPUT = $(KERNEL_BIN_DIR)/kernel.elf

KERNEL_SRC = \
	$(call recursive_wildcard, $(KERNEL_SRC_DIR), *.c) \
	$(call recursive_wildcard, $(KERNEL_SRC_DIR), *.s) \
	$(LIBC_FUNCTIONS)/string/memcpy.c \
	$(LIBC_FUNCTIONS)/string/memset.c \
	$(LIBC_FUNCTIONS)/string/memcmp.c \
	$(LIBC_FUNCTIONS)/string/memmove.c \
	$(LIBC_FUNCTIONS)/string/strlen.c \
	$(LIBC_FUNCTIONS)/string/strcmp.c \
	$(LIBC_FUNCTIONS)/string/strcpy.c \
	$(LIBC_FUNCTIONS)/ctype/isalnum.c \
	$(LIBC_FUNCTIONS)/ctype/isdigit.c \
	$(LIBC_FUNCTIONS)/ctype/isalpha.c \

KERNEL_OBJECTS = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/%.o, $(KERNEL_SRC))

$(KERNEL_BUILD_DIR)/%.c.o: $(KERNEL_SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(KERNEL_C_FLAGS) -I $(KERNEL_SRC_DIR) -c -o $@ $<

$(KERNEL_BUILD_DIR)/%.s.o: $(KERNEL_SRC_DIR)/%.s
	@mkdir -p $(@D)
	$(ASM) $(ASM_FLAGS) $^ -o $@

$(KERNEL_OUTPUT): $(KERNEL_OBJECTS)	
	@echo $(KERNEL_SRC)
	@mkdir -p $(@D)
	@$(LD) $(LD_FLAGS) -T $(KERNEL_SRC_DIR)/linker.ld -o $@ $^

BUILD += $(KERNEL_OUTPUT)