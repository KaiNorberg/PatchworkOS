NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/kernel

# Only add the non user libstd files
SRC += \
	$(call find_sources,src/libstd/common) \
	$(call find_sources,src/libstd/functions)

CFLAGS += \
	$(CFLAGS_DISABLE_SIMD)  \
	-fno-pic \
	-fno-stack-check \
	-mcmodel=kernel \
	-mno-red-zone \
	-Isrc/libstd \
	-D__KERNEL__ \
	-D__STDC_WANT_LIB_EXT1__=1

# Will cause a panic to trigger QEMU exit for testing purposes
ifeq ($(DEBUG),1)
    CFLAGS += -DQEMU_ISA_DEBUG_EXIT
endif

ASFLAGS += -D__KERNEL__ -Isrc/libstd

LDFLAGS += -T$(SRCDIR)/linker.lds -z max-page-size=0x1000 -z norelro

all: $(TARGET)

.PHONY: all

include Make.rules
