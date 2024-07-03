include Make.defaults

TARGET := $(BINDIR)/taskbar.elf

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
