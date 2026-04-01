COMP_NAME = freetype
COMP_VERSION = 1.0.0
COMP_TYPE = port
COMP_DEPENDS = libc libm libpng zlib

include $(COMP_DIR)/Make.comp.defaults

define PORT_BUILD_CMD
	@if [ ! -d "$(COMP_BUILD_DIR)/src" ]; then \
		echo "  CLONE $(COMP_NAME)"; \
		git clone https://gitlab.freedesktop.org/freetype/freetype.git $(COMP_BUILD_DIR)/src >/dev/null 2>&1; \
	fi
	@echo "  BUILD $(COMP_NAME)"
	@mkdir -p $(COMP_BUILD_DIR)/build
	@cd $(COMP_BUILD_DIR)/build && \
		cmake ../src \
			-DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) \
			-DCMAKE_INSTALL_PREFIX=/ \
			-DBUILD_SHARED_LIBS=ON \
			-DFT_DISABLE_BZIP2=ON \
			-DFT_DISABLE_HARFBUZZ=ON \
			-DFT_DISABLE_BROTLI=ON && \
		$(MAKE)
endef

define PORT_STAGE_CMD
	@echo "  STAGE $(COMP_NAME) (install)"
	@$(MAKE) -C $(COMP_BUILD_DIR)/build DESTDIR=$(STAGE_DIR) install

	@if [ -d "$(STAGE_DIR)/usr" ]; then \
		cp -f -r $(STAGE_DIR)/usr/* $(STAGE_DIR)/ 2>/dev/null || true; \
		rm -rf $(STAGE_DIR)/usr; \
	fi

	@if [ -d "$(STAGE_DIR)/include/freetype2" ]; then \
		mv -f $(STAGE_DIR)/include/freetype2/* $(STAGE_DIR)/include/; \
		rm -rf $(STAGE_DIR)/include/freetype2; \
	fi
endef

include $(COMP_DIR)/Make.comp.rules
