COMP_NAME = reduct
COMP_VERSION = 1.0.0
COMP_TYPE = port
COMP_DEPENDS = libc libm

include $(COMP_DIR)/Make.comp.defaults

define PORT_BUILD_CMD
	@if [ ! -d "$(COMP_BUILD_DIR)/src" ]; then \
		echo "  CLONE $(COMP_NAME)"; \
		git clone https://github.com/KaiNorberg/Reduct.git $(COMP_BUILD_DIR)/src >/dev/null 2>&1; \
	fi
	@echo "  BUILD $(COMP_NAME)"
	@cd $(COMP_BUILD_DIR)/src && \
		cmake -B ../build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) && \
		cmake --build ../build
endef

define PORT_STAGE_CMD
	@echo "  STAGE $(COMP_NAME)"
	@cd $(COMP_BUILD_DIR)/build && \
		cmake --install . --prefix $(STAGE_DIR)
endef

include $(COMP_DIR)/Make.comp.rules
