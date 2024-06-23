KERNEL_OUT = $(BIN_DIR)/kernel/kernel.elf

KERNEL_SRC = \
	$(wildcard $(SRC_DIR)/kernel/*.c) \
	$(wildcard $(SRC_DIR)/kernel/*.s) \
	$(wildcard $(SRC_DIR)/stdlib/*.c) \
	$(wildcard $(SRC_DIR)/stdlib/*.s)

KERNEL_OBJ = $(patsubst $(SRC_DIR)/%, $(BUILD_DIR)/kernel/%.o, $(KERNEL_SRC))

KERNEL_C_FLAGS = $(BASE_C_FLAGS) \
	-fno-pic -mcmodel=large \
	-fno-stack-protector \
	-mno-mmx -mno-3dnow \
	-mno-80387 -mno-sse \
	-mno-sse2 -mno-sse3 \
	-mno-ssse3 -mno-sse4 \
	-D__EMBED__ \
	-I$(INCLUDE_DIR)/kernel \
	-I$(SRC_DIR)/kernel

KERNEL_ASM_FLAGS = $(BASE_ASM_FLAGS) \
	-D__EMBED__ \
	-I$(INCLUDE_DIR)/kernel \
	-I$(SRC_DIR)/kernel

$(BUILD_DIR)/kernel/%.c.o: $(SRC_DIR)/%.c
	$(MKCWD)
	$(CC) $(KERNEL_C_FLAGS) -c -o $@ $<

$(BUILD_DIR)/kernel/%.s.o: $(SRC_DIR)/%.s
	$(MKCWD)
	$(ASM) $(KERNEL_ASM_FLAGS) $^ -o $@

$(KERNEL_OUT): $(KERNEL_OBJ)
	$(MKCWD)
	$(LD) $(BASE_LD_FLAGS) -T$(SRC_DIR)/kernel/linker.ld -o $@ $^

BUILD += $(KERNEL_OUT)
