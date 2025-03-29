include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
