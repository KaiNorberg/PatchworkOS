include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

LDFLAGS += -Lbin/libstd -Lbin/libdwm -lstd -ldwm

all: $(TARGET)

.PHONY: all

include Make.rules
