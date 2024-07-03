include Make.defaults

TARGET := $(BINDIR)/shell.elf

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
