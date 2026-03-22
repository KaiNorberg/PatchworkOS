include Make.defaults

KERNEL_SRC = $(ROOT_DIR)/kernel
KERNEL_BUILD = $(BUILD_DIR)/kernel
KERNEL_BIN = $(BIN_DIR)/kernel/kernel

# Only add the non user libstd files
SRC := \
	$(shell find $(KERNEL_SRC) -name '*.c' -o -name '*.S') \
	$(shell find $(LIBSTD_DIR)/src/common -name '*.c' -o -name '*.S') \
	$(shell find $(LIBSTD_DIR)/src/functions -name '*.c' -o -name '*.S')

OBJ_KERNEL := $(patsubst $(KERNEL_SRC)/%, $(KERNEL_BUILD)/%.o, $(filter $(KERNEL_SRC)/%, $(SRC)))
OBJ_LIBSTD := $(patsubst $(LIBSTD_DIR)/%, $(BUILD_DIR)/libstd/%.o, $(filter $(LIBSTD_DIR)/%, $(SRC)))
OBJ := $(OBJ_KERNEL) $(OBJ_LIBSTD)

CFLAGS += \
	$(CFLAGS_DISABLE_SIMD)  \
	-fno-pic \
	-fno-stack-check \
	-mcmodel=kernel \
	-mno-red-zone \
	-I$(LIBSTD_DIR)/src \
	-D_KERNEL_ \
	-D__STDC_WANT_LIB_EXT1__=1

# Will cause a panic to trigger QEMU exit for testing purposes
ifeq ($(QEMU_EXIT_ON_PANIC),1)
    CFLAGS += -DQEMU_EXIT_ON_PANIC
endif

ASFLAGS += \
	-I$(LIBSTD_DIR)/src \
	-D_PATCHWORK_OS_ \
	-D_KERNEL_ \
	-D__STDC_WANT_LIB_EXT1__=1

LDFLAGS += \
	-no-pie \
	-z noexecstack \
	-z max-page-size=0x1000 \
	-z norelro \
	-T$(ROOT_DIR)/kernel/linker.lds

all: $(KERNEL_BIN)

$(KERNEL_BUILD)/%.c.o: $(KERNEL_SRC)/%.c
	$(MKCWD)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(KERNEL_BUILD)/%.S.o: $(KERNEL_SRC)/%.S
	$(MKCWD)
	@echo "  AS    $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/libstd/%.c.o: $(LIBSTD_DIR)/%.c
	$(MKCWD)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(BUILD_DIR)/libstd/%.S.o: $(LIBSTD_DIR)/%.S
	$(MKCWD)
	@echo "  AS    $<"
	@$(CC) $(ASFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJ)
	$(MKCWD)
	@echo "  LD    $@"
	@$(LD) $(LDFLAGS) -o $@ $^
