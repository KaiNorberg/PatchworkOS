#include Make.defaults

TARGET := $(BINDIR)/$(BOX)

all: $(TARGET)

$(TARGET):
	@if [ ! -d "lib/doomgeneric" ]; then \
	    git clone https://github.com/KaiNorberg/doomgeneric-patchworkos lib/doomgeneric; \
	fi
	$(MAKE) -C lib/doomgeneric/doomgeneric -f Makefile.patchwork

#include Make.rules
