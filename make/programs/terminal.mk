include Make.defaults

TARGET := $(BINDIR)/terminal

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
