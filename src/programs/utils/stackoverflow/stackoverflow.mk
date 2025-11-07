include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

LDFLAGS +=

CFLAGS += -fno-stack-protector -Wno-infinite-recursion -O0

all: $(TARGET)

.PHONY: all

include Make.rules
