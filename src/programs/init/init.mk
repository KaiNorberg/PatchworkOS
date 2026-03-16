NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

CRT_START := bin/libstd/crt0.o bin/libstd/crti.o
CRT_END := bin/libstd/crtn.o
LDSTDLIB := bin/libstd/libstd.a

all: $(TARGET)

.PHONY: all

include Make.rules
