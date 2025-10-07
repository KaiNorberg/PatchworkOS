NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/kernel

SRC += $(wildcard src/libstd/*.c) $(wildcard src/libstd/*.s) \
 	$(wildcard src/libstd/functions/**/*.c) $(wildcard src/libstd/functions/**/*.s) \
	$(wildcard src/libstd/common/*.c) $(wildcard src/libstd/common/*.s) \
	$(wildcard src/libstd/platform/kernel/*.c) $(wildcard src/libstd/platform/kernel/*.s) \
	$(wildcard src/libstd/platform/kernel/functions/**/*.c) $(wildcard src/libstd/platform/kernel/functions/**/*.s) \

CFLAGS += $(CFLAGS_DISABLE_SIMD) -fno-pic -fno-stack-check -mcmodel=kernel \
	-mno-red-zone \
	-Isrc/libstd \
	-D__KERNEL__ \
	-D__STDC_WANT_LIB_EXT1__=1

ifeq ($(DEBUG_MODE),1)
    CFLAGS += -DQEMU_ISA_DEBUG_EXIT
endif

ASFLAGS += -D__KERNEL__ -Isrc/libstd

LDFLAGS += -T$(SRCDIR)/linker.lds -z max-page-size=0x1000 -z norelro

all: $(TARGET)

generate_aml_test:
	if [ ! -f src/kernel/acpi/aml/_aml_full_test.h ]; then \
        echo "Generating _aml_full_test.h from full.aml"; \
        xxd -i lib/aslts/full.aml > src/kernel/acpi/aml/_aml_full_test.h; \
    else \
        echo "_aml_full_test.h already exists, skipping generation"; \
    fi

delete_aml_test:
	rm -f src/kernel/acpi/aml/aml_test.h

.PHONY: all

include Make.rules
