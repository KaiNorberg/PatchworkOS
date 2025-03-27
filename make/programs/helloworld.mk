include Make.defaults

TARGET := $(BINDIR)/helloworld

LDFLAGS += -Lbin/stdlib -lstd

all: $(TARGET)

.PHONY: all

include Make.rules
