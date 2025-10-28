include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

LDFLAGS +=

CFLAGS += -fno-stack-protector -Wno-infinite-recursion

all: $(TARGET)

.PHONY: all

include Make.rules
