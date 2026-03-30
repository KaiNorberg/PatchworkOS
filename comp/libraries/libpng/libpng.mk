COMP_NAME = libpng
COMP_VERSION = 1.0.0
COMP_TYPE = port
COMP_DEPENDS = libc libm zlib

include $(COMP_DIR)/Make.comp.defaults

define PORT_BUILD_CMD
	@if [ ! -d "$(COMP_BUILD_DIR)/src" ]; then \
		echo "  CLONE $(COMP_NAME)"; \
		git clone https://github.com/glennrp/libpng.git $(COMP_BUILD_DIR)/src >/dev/null 2>&1; \
	fi
	@echo "  BUILD $(COMP_NAME)"
	@mkdir -p $(COMP_BUILD_DIR)/build
	@cd $(COMP_BUILD_DIR)/build && \
		cmake ../src \
			-DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) \
			-DCMAKE_INSTALL_PREFIX=/ \
			-DPNG_SHARED=ON \
			-DPNG_STATIC=ON \
			-DPNG_TESTS=OFF \
			-DPNG_TOOLS=OFF && \
		$(MAKE)
endef

define PORT_STAGE_CMD
	@echo "  STAGE $(COMP_NAME) (install)"
	@$(MAKE) -C $(COMP_BUILD_DIR)/build DESTDIR=$(STAGE_DIR) install

	@if [ -d "$(STAGE_DIR)/usr" ]; then \
		cp -r $(STAGE_DIR)/usr/* $(STAGE_DIR)/ 2>/dev/null || true; \
		rm -rf $(STAGE_DIR)/usr; \
	fi

	@if [ -d "$(STAGE_DIR)/include/libpng16" ]; then \
		mv $(STAGE_DIR)/include/libpng16/* $(STAGE_DIR)/include/; \
		rmdir $(STAGE_DIR)/include/libpng16; \
	fi
endef

include $(COMP_DIR)/Make.comp.rules
