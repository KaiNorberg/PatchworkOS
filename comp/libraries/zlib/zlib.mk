COMP_NAME = zlib
COMP_VERSION = 1.3.1
COMP_TYPE = port
COMP_DEPENDS = libc

include $(COMP_DIR)/Make.comp.defaults

define PORT_BUILD_CMD
	@if [ ! -d "$(COMP_BUILD_DIR)/src" ]; then \
		echo "  CLONE $(COMP_NAME)"; \
		git clone https://github.com/madler/zlib.git $(COMP_BUILD_DIR)/src >/dev/null 2>&1; \
	fi
	@echo "  BUILD $(COMP_NAME)"
	@cd $(COMP_BUILD_DIR)/src && \
		CC="$(PORT_CC)" \
		AR="$(PORT_AR)" \
		RANLIB="$(PORT_RANLIB)" \
		CFLAGS="$(CFLAGS)" \
		LDFLAGS="$(LDFLAGS) $(LDSTDLIB)" \
		LDSHARED="$(PORT_CC) -shared -Wl,-soname,libz.so" \
		./configure --prefix=/ --solo --shared && \
		$(MAKE) libz.a
endef

define PORT_STAGE_CMD
	@echo "  STAGE $(COMP_NAME) (install)"
	@$(MAKE) -C $(COMP_BUILD_DIR)/src DESTDIR=$(STAGE_DIR) install
endef

include $(COMP_DIR)/Make.comp.rules
