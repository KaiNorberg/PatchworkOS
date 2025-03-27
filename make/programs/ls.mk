include Make.defaults

TARGET := $(BINDIR)/ls

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
