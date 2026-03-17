NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/init

CRT_START := bin/libstd/crt0.o bin/libstd/crti.o
CRT_END := bin/libstd/crtn.o
LDSTDLIB := bin/libstd/libstd.a

# Since the init process cant have its stack setup by any parent, the init process must do it on its own. 
# Therefore, we override the entry point to a special early initialization function.
LDFLAGS += -e _start_early

all: $(TARGET)

.PHONY: all

include Make.rules
