include Make.defaults

TARGET := $(BINDIR)/cursor.elf

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules