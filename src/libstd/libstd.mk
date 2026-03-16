NOSTDLIB=1
include Make.defaults

CRT_OBJ := $(BINDIR)/crt0.o $(BINDIR)/crti.o $(BINDIR)/crtn.o

TARGET := $(BINDIR)/libstd.so $(BINDIR)/libstd.a $(CRT_OBJ)

SRC = \
	$(call find_sources,src/libstd/common) \
	$(call find_sources,src/libstd/functions) \
	$(call find_sources,src/libstd/user/common) \
	$(call find_sources,src/libstd/user/functions) \
	src/libstd/user/user.c

ASFLAGS += -Isrc/libstd -fPIC

CFLAGS += -D__STDC_WANT_LIB_EXT1__=1 -fPIC

all: $(TARGET)

.PHONY: all

$(BINDIR)/crt%.o: src/libstd/user/crt/crt%.S
	$(MKCWD)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) -c -o $@ $<

include Make.rules
