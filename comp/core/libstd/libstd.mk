COMP_NAME = libstd
COMP_VERSION = 1.0.0
COMP_TYPE = static-shared
COMP_DEPENDS =

include $(COMP_DIR)/Make.comp.defaults

CRT_OBJ := $(COMP_BUILD_DIR)/user/crt/crt0.S.o $(COMP_BUILD_DIR)/user/crt/crti.S.o $(COMP_BUILD_DIR)/user/crt/crtn.S.o

SRC := \
	$(shell find $(SRC_DIR)/common -name '*.c' -o -name '*.S') \
	$(shell find $(SRC_DIR)/functions -name '*.c' -o -name '*.S') \
	$(shell find $(SRC_DIR)/user -name '*.c' -o -name '*.S')

ASFLAGS += \
	-fPIC \
	-I$(LIBSTD_DIR)/src \
	-D_PATCHWORK_OS_ \
	-D__STDC_WANT_LIB_EXT1__=1 \
	-Isrc/libstd

LDFLAGS += \
	-z noexecstack \
	-Wno-unused-command-line-argument \
	-shared

LDSTDLIB :=

CFLAGS += \
	-fPIC \
	-I$(LIBSTD_DIR)/src \
	-D__STDC_WANT_LIB_EXT1__=1

include $(COMP_DIR)/Make.comp.rules

before-stage:
	# Copy crt0.o, crti.o, crtn.o to stage
	@cp $(CRT_OBJ) $(COMP_BIN_DIR)
	@mv $(COMP_BIN_DIR)/crt0.S.o $(COMP_BIN_DIR)/crt0.o
	@mv $(COMP_BIN_DIR)/crti.S.o $(COMP_BIN_DIR)/crti.o
	@mv $(COMP_BIN_DIR)/crtn.S.o $(COMP_BIN_DIR)/crtn.o
