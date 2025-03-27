include Make.defaults

TARGET := $(BINDIR)/threadtest

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
