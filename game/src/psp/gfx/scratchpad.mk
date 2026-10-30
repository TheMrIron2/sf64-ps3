SCRATCHPAD_TOOL := src/psp/gfx/scratchpad_code.py
SCRATCHPAD_BACKEND := $(BUILD_DIR)/src/psp/gfx/gfx_psp_backend_gu.o
SCRATCHPAD_DL := $(BUILD_DIR)/src/psp/gfx/gfx_psp_dl.o
LDFLAGS += -Wl,--wrap=PspGfxGuDevice_BeginFrame,--wrap=PspGfxGuDevice_Submit
LDFLAGS += -Wl,--wrap=PspGfxGuDevice_Shutdown

define scratchpad_object
$(1): $(2) $(SCRATCHPAD_TOOL) src/psp/gfx/scratchpad.mk $(COMPILE_FLAGS_STAMP)
	@mkdir -p $$(@D)
	$$(CC) -S $$(CFLAGS) $$(PSP_WARNINGS) -Isrc/psp/gfx/ -MMD -MP -MT$$@ -MF$$(@:.o=.d) -o $$(BUILD_DIR)/scratchpad-$(3)-original.s $$<
	$$(PYTHON) $$(SCRATCHPAD_TOOL) prepare $$(BUILD_DIR)/scratchpad-$(3)-original.s $$(BUILD_DIR)/scratchpad-$(3)-dispatch.s
	$$(CC) -G0 -c -o $$(BUILD_DIR)/scratchpad-$(3)-original.o $$(BUILD_DIR)/scratchpad-$(3)-original.s
	$$(CC) -G0 -c -o $$@ $$(BUILD_DIR)/scratchpad-$(3)-dispatch.s
endef
$(eval $(call scratchpad_object,$(SCRATCHPAD_BACKEND),src/psp/gfx/gfx_psp_backend_gu.c,backend))
$(eval $(call scratchpad_object,$(SCRATCHPAD_DL),src/psp/gfx/gfx_psp_dl.c,dl))

$(BUILD_DIR)/gfx_psp_scratchpad_code.h: $(SCRATCHPAD_BACKEND) $(SCRATCHPAD_DL) $(SCRATCHPAD_TOOL)
	$(PYTHON) $(SCRATCHPAD_TOOL) header $(BUILD_DIR)

$(BUILD_DIR)/src/psp/gfx/gfx_psp_scratchpad.o: $(BUILD_DIR)/gfx_psp_scratchpad_code.h
