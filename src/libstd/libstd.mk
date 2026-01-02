NOSTDLIB=1
include Make.defaults

TARGET := $(BINDIR)/libstd.a

SRC = \
	$(call find_sources,src/libstd/common) \
	$(call find_sources,src/libstd/functions) \
	$(call find_sources,src/libstd/user/common) \
	$(call find_sources,src/libstd/user/functions) \
	src/libstd/user/user.c

ASFLAGS += -Isrc/libstd

CFLAGS += -D__STDC_WANT_LIB_EXT1__=1

all: $(TARGET)

.PHONY: all

include Make.rules
