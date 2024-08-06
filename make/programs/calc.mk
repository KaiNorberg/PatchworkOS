include Make.defaults

TARGET := $(BINDIR)/calc

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
