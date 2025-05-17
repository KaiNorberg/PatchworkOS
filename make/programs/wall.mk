include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

LDFLAGS += -Lbin/libdwm -ldwm

all: $(TARGET)

.PHONY: all

include Make.rules
