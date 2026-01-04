#include Make.defaults

TARGET := $(BINDIR)/$(BOX)

$(TARGET):
	@if [ ! -d "lib/doomgeneric" ]; then \
	    git clone https://github.com/KaiNorberg/doomgeneric-patchworkos lib/doomgeneric; \
	fi
	$(MAKE) -C lib/doomgeneric/doomgeneric -f Makefile.patchwork

.PHONY: $(TARGET)

#include Make.rules
