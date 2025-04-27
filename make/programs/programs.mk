include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

LDFLAGS += -Lbin/stdlib -Lbin/win -lstd -lwin

all: $(TARGET)

.PHONY: all

include Make.rules
