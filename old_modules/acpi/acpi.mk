NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/$(MODULE)

CFLAGS += $(CFLAGS_MODULE)

ASFLAGS += $(ASFLAGS_MODULE)

LDFLAGS += $(LDFLAGS_MODULE)

ifeq ($(ACPI_NOTEST),1)
	CFLAGS += -D_ACPI_NOTEST_
endif

all: $(TARGET)

.PHONY: all

include Make.rules
