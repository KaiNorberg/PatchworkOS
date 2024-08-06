include Make.defaults

TARGET := $(BINDIR)/shell

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
