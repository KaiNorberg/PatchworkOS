include Make.defaults

INIT_SRC = $(ROOT_DIR)/init
INIT_BUILD = $(BUILD_DIR)/init
INIT_BIN = $(BIN_DIR)/init

TARGET := $(INIT_BIN)/init

SRC := $(shell find $(INIT_SRC) -name '*.c' -o -name '*.S')

OBJ = $(patsubst $(INIT_SRC)/%, $(INIT_BUILD)/%.o, $(SRC))

ASFLAGS += \
	-I$(LIBSTD_DIR)/src \
	-D_PATCHWORK_OS_ \
	-D__STDC_WANT_LIB_EXT1__=1

# Since the init process cant have its stack setup by any parent, the init process must do it on its own.
# Therefore, we override the entry point to a special early initialization function.
LDFLAGS += \
	-static \
	-lc \
	-e _start_early \
	-z noexecstack \
	-L$(BIN_DIR)/comp

CFLAGS += \
	-I$(LIBSTD_DIR)/src \
	-D__STDC_WANT_LIB_EXT1__=1

all: $(TARGET)

$(INIT_BUILD)/%.c.o: $(INIT_SRC)/%.c
	$(MKCWD)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(INIT_BUILD)/%.S.o: $(INIT_SRC)/%.S
	$(MKCWD)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

$(TARGET): $(OBJ)
	$(MKCWD)
	@echo "  LD    $@"
	@$(LD) -o $@ $(CRT_START) $^ $(LDFLAGS) $(CRT_END)

-include $(OBJ:.o=.d)
