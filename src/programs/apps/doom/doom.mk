#include Make.defaults

TARGET := $(BINDIR)/$(PROGRAM)

$(TARGET):
	@if [ ! -d "lib/doomgeneric-patchworkos" ]; then \
	    git clone https://github.com/KaiNorberg/doomgeneric-patchworkos lib/doomgeneric-patchworkos; \
	fi
	$(MAKE) -C lib/doomgeneric-patchworkos/doomgeneric -f Makefile.patchwork

.PHONY: $(TARGET)

#include Make.rules
