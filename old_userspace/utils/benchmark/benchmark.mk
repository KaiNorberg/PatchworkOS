include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

LDFLAGS += 
CFLAGS += -O0 -Wno-infinite-recursion

all: $(TARGET)

.PHONY: all

include Make.rules
