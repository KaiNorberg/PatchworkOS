NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/kernel

SRC += $(wildcard src/libstd/*.c) $(wildcard src/libstd/*.s) \
 	$(wildcard src/libstd/functions/**/*.c) $(wildcard src/libstd/functions/**/*.s) \
	$(wildcard src/libstd/common/*.c) $(wildcard src/libstd/common/*.s)

CFLAGS += $(CFLAGS_DISABLE_SIMD) -fno-pic -fno-stack-check -mcmodel=kernel \
	-mno-red-zone \
	-Isrc/libstd \
	-D__KERNEL__ \
	-D__STDC_WANT_LIB_EXT1__=1

ifeq ($(DEBUG),1)
    CFLAGS += -DQEMU_ISA_DEBUG_EXIT
endif

ASFLAGS += -D__KERNEL__ -Isrc/libstd

LDFLAGS += -T$(SRCDIR)/linker.lds -z max-page-size=0x1000 -z norelro

all: $(TARGET)

.PHONY: all

include Make.rules
