include make/common.mk

.DEFAULT_GOAL := all

N64PS3_PS3_MK := $(N64PS3_DIR)/make/n64ps3_ps3.mk
ifneq ($(wildcard $(N64PS3_PS3_MK)),)
include $(N64PS3_DIR)/make/n64ps3_ps3.mk
endif

PS3_BUILD_DIR := $(BUILD_ROOT)/ps3
PS3_N64PS3_BACKEND_BUILD_DIR := $(PS3_BUILD_DIR)/n64ps3/src/backends/ps3
PS3_TARGET_ELF := $(PS3_BUILD_DIR)/$(SF64_PS3_TARGET).elf
PS3_TARGET_SELF := $(PS3_BUILD_DIR)/$(SF64_PS3_TARGET).self
PS3_TARGET_FAKE_SELF := $(PS3_BUILD_DIR)/$(SF64_PS3_TARGET).fake.self
PS3_SELF_INPUT_ELF := $(PS3_BUILD_DIR)/self/$(SF64_PS3_TARGET).elf
PS3_OBJECTS := $(patsubst src/%.c,$(PS3_BUILD_DIR)/%.o,$(SF64_COMMON_C_FILES))
PS3_SHADER_CACHE_DIR ?= $(PS3_N64PS3_BACKEND_BUILD_DIR)
SF64_PS3_ARWING ?= 0
SF64_PS3_ARWING_ROM ?= baserom.us.rev1.z64
SF64_SOURCE_DIR ?= ../sf64-psp

PS3_N64PS3_OBJECTS := $(patsubst $(N64PS3_DIR)/%.c,$(PS3_BUILD_DIR)/n64ps3/%.o,$(N64PS3_C_FILES))
PS3_DEPENDENCIES := $(PS3_OBJECTS:.o=.d) $(PS3_N64PS3_OBJECTS:.o=.d)
PS3_TRIANGLE_VPO := $(PS3_SHADER_CACHE_DIR)/rdprsx_triangle.vpo
PS3_TRIANGLE_FPO := $(PS3_SHADER_CACHE_DIR)/rdprsx_triangle.fpo
PS3_TRIANGLE_VPO_HEADER := $(PS3_SHADER_CACHE_DIR)/rdprsx_triangle_vpo.h
PS3_TRIANGLE_FPO_HEADER := $(PS3_SHADER_CACHE_DIR)/rdprsx_triangle_fpo.h
PS3_TRIANGLE_SHADER_HEADERS := $(PS3_TRIANGLE_VPO_HEADER) $(PS3_TRIANGLE_FPO_HEADER)

PPU_CC ?= $(PS3DEV)/ppu/bin/ppu-gcc
PPU_STRIP ?= $(PS3DEV)/ppu/bin/ppu-strip
SPRX_LINKER ?= $(PS3DEV)/bin/sprxlinker
MAKE_SELF ?= $(PS3DEV)/bin/make_self
FSELF ?= $(PS3DEV)/bin/fself
CGCOMP ?= $(PS3DEV)/bin/cgcomp
CGCOMP_RUNTIME_DIR ?=
CGCOMP_LD_LIBRARY_PATH ?= $(if $(CGCOMP_RUNTIME_DIR),$(CGCOMP_RUNTIME_DIR)$(if $(LD_LIBRARY_PATH),:$(LD_LIBRARY_PATH)),$(LD_LIBRARY_PATH))
PYTHON ?= python3

PS3_CPPFLAGS := \
	$(addprefix -I,$(SF64_INCLUDE_DIRS)) \
	$(N64PS3_PS3_CPPFLAGS) \
	-I$(PSL1GHT)/ppu/include \
	-I$(PSL1GHT)/ppu/include/simdmath \
	-I$(PS3_N64PS3_BACKEND_BUILD_DIR) \
	-I$(PS3_SHADER_CACHE_DIR) \
	-DSF64_PS3_TARGET=1
PS3_CPPFLAGS += $(PS3_EXTRA_CPPFLAGS)

ifeq ($(SF64_PS3_ARWING),1)
PS3_CPPFLAGS += -DSF64_PS3_ARWING_PROBE=1
PS3_ARWING_RESOURCE_C := $(PS3_BUILD_DIR)/resources/ast_arwing.c
PS3_ARWING_RESOURCE_OBJECT := $(PS3_BUILD_DIR)/resources/ast_arwing.o
PS3_TITLE_STARFIELD_OBJECT := $(PS3_BUILD_DIR)/sf64_title_starfield.o
PS3_ORIGINAL_STARFIELD_C := $(PS3_BUILD_DIR)/original_title_starfield.c
PS3_ORIGINAL_STARFIELD_OBJECT := $(PS3_BUILD_DIR)/original_title_starfield.o
PS3_TITLE_RESOURCE_C := $(PS3_BUILD_DIR)/resources/ast_title.c
PS3_TITLE_RESOURCE_OBJECT := $(PS3_BUILD_DIR)/resources/ast_title.o
PS3_ORIGINAL_TITLE_LOGIC_C := $(PS3_BUILD_DIR)/original_title_logic.c
PS3_ORIGINAL_TITLE_LOGIC_OBJECT := $(PS3_BUILD_DIR)/original_title_logic.o
PS3_TITLE_RUNTIME_OBJECT := $(PS3_BUILD_DIR)/sf64_title_runtime.o
PS3_ORIGINAL_TITLE_SOURCE_FILES := \
	$(SF64_SOURCE_DIR)/src/engine/fox_bg.c \
	$(SF64_SOURCE_DIR)/src/engine/fox_play.c \
	$(SF64_SOURCE_DIR)/src/engine/fox_std_lib.c \
	$(SF64_SOURCE_DIR)/src/overlays/ovl_menu/fox_title.c \
	$(SF64_SOURCE_DIR)/src/libc_math64.c \
	$(SF64_SOURCE_DIR)/src/sys/sys_math.c \
	$(SF64_SOURCE_DIR)/src/sys/sys_memory.c \
	$(SF64_SOURCE_DIR)/assets/yaml/us/rev1/ast_title.yaml
PS3_ORIGINAL_TITLE_CPPFLAGS := \
	-I$(SF64_SOURCE_DIR)/include \
	-I$(SF64_SOURCE_DIR) \
	-I$(SF64_SOURCE_DIR)/src \
	-I$(SF64_SOURCE_DIR)/src/overlays/ovl_menu \
	-D_LANGUAGE_C -D_MIPS_SZLONG=32
PS3_OBJECTS += $(PS3_ARWING_RESOURCE_OBJECT) $(PS3_TITLE_RESOURCE_OBJECT) \
	$(PS3_TITLE_STARFIELD_OBJECT) $(PS3_ORIGINAL_STARFIELD_OBJECT) \
	$(PS3_ORIGINAL_TITLE_LOGIC_OBJECT) $(PS3_TITLE_RUNTIME_OBJECT)
PS3_DEPENDENCIES += $(PS3_TITLE_STARFIELD_OBJECT:.o=.d) \
	$(PS3_ORIGINAL_STARFIELD_OBJECT:.o=.d) \
	$(PS3_ORIGINAL_TITLE_LOGIC_OBJECT:.o=.d) $(PS3_TITLE_RUNTIME_OBJECT:.o=.d)
endif

PSL1GHT_PPU_MACHDEP ?= -mhard-float -fmodulo-sched -ffunction-sections -fdata-sections
PS3_CFLAGS ?= -std=gnu99 -Wall -Wextra -Werror -O2 -mcpu=cell $(PSL1GHT_PPU_MACHDEP)
PS3_CFLAGS += -MMD -MP
PS3_LDFLAGS ?= -mcpu=cell $(PSL1GHT_PPU_MACHDEP) -L$(PSL1GHT)/ppu/lib -L$(PS3DEV)/ppu/lib
ifeq ($(SF64_PS3_ARWING),1)
PS3_LDFLAGS += -Wl,--gc-sections
endif
N64PS3_PS3_LDLIBS ?= -lgcm_sys -lrsx -lio -lsysutil -lm -lrt -llv2
PS3_LDLIBS ?=

-include $(PS3_DEPENDENCIES)

.PHONY: all self fake-self check-ps3-env check-fself check-self-tools check-cgcomp check-n64ps3 check-original-title-source

all: check-ps3-env check-n64ps3 $(PS3_TARGET_ELF)
	@printf '%s\n' "built PS3 smoke ELF: $(PS3_TARGET_ELF)"
	@printf '%s\n' "use 'make ps3-self' to create an RPCS3-loadable SELF."
	@printf '%s\n' "a SELF is not an installable PKG."

self: all check-self-tools $(PS3_TARGET_SELF)
	@printf '%s\n' "built PS3 SELF: $(PS3_TARGET_SELF)"

fake-self: all check-fself $(PS3_TARGET_FAKE_SELF)
	@printf '%s\n' "built PS3 fake SELF: $(PS3_TARGET_FAKE_SELF)"

check-ps3-env:
	@if [ -z "$(PS3DEV)" ]; then \
		printf '%s\n' "error: PS3DEV is not set."; \
		printf '%s\n' "setup: export PS3DEV=/path/to/ps3dev"; \
		exit 2; \
	fi
	@if [ -z "$(PSL1GHT)" ]; then \
		printf '%s\n' "error: PSL1GHT is not set."; \
		printf '%s\n' "setup: export PSL1GHT=/path/to/PSL1GHT"; \
		exit 2; \
	fi
	@if [ ! -x "$(PPU_CC)" ]; then \
		printf '%s\n' "error: ppu-gcc not found or not executable at $(PPU_CC)."; \
		printf '%s\n' "check PS3DEV or override PPU_CC=/path/to/ppu-gcc"; \
		exit 2; \
	fi

check-fself:
	@if [ ! -x "$(FSELF)" ]; then \
		printf '%s\n' "error: PSL1GHT fself tool not found or not executable at $(FSELF)."; \
		printf '%s\n' "check PS3DEV or override FSELF=/path/to/fself"; \
		exit 2; \
	fi

check-self-tools:
	@if [ ! -x "$(PPU_STRIP)" ]; then \
		printf '%s\n' "error: PPU strip tool not found or not executable at $(PPU_STRIP)."; \
		exit 2; \
	fi
	@if [ ! -x "$(SPRX_LINKER)" ]; then \
		printf '%s\n' "error: PSL1GHT sprxlinker not found or not executable at $(SPRX_LINKER)."; \
		exit 2; \
	fi
	@if [ ! -x "$(MAKE_SELF)" ]; then \
		printf '%s\n' "error: PSL1GHT make_self not found or not executable at $(MAKE_SELF)."; \
		exit 2; \
	fi

check-cgcomp:
	@if [ ! -x "$(CGCOMP)" ]; then \
		printf '%s\n' "error: PSL1GHT cgcomp not found or not executable at $(CGCOMP)."; \
		exit 2; \
	fi
	@if ! LD_LIBRARY_PATH='$(CGCOMP_LD_LIBRARY_PATH)' $(CGCOMP) -h >/dev/null 2>&1; then \
		printf '%s\n' "error: cgcomp cannot load libCg.so."; \
		printf '%s\n' "install NVIDIA Cg Toolkit 3.1 or set CGCOMP_RUNTIME_DIR to its library directory."; \
		exit 2; \
	fi

check-n64ps3:
	@if [ ! -d "$(N64PS3_DIR)" ]; then \
		printf '%s\n' "error: $(N64PS3_DIR) is missing."; \
		printf '%s\n' "setup: git submodule add https://github.com/TheMrIron2/n64ps3 $(N64PS3_DIR)"; \
		exit 2; \
	fi
	@if [ ! -d "$(N64PS3_DIR)/include" ]; then \
		printf '%s\n' "error: $(N64PS3_DIR) exists but has no include/ directory."; \
		printf '%s\n' "n64ps3 is not buildable/consumable by this PS3 smoke target yet."; \
		exit 2; \
	fi
	@if [ ! -f "$(N64PS3_PS3_MK)" ]; then \
		printf '%s\n' "error: $(N64PS3_PS3_MK) is missing."; \
		printf '%s\n' "update extern/n64ps3 to a revision with Makefile exports."; \
		exit 2; \
	fi

$(PS3_TARGET_ELF): $(PS3_OBJECTS) $(PS3_N64PS3_OBJECTS)
	$(PPU_CC) $(PS3_LDFLAGS) -o $@ $^ $(N64PS3_PS3_LDLIBS) $(PS3_LDLIBS)

check-original-title-source:
	@if [ ! -f "$(SF64_SOURCE_DIR)/src/engine/fox_bg.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/engine/fox_play.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/engine/fox_std_lib.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/overlays/ovl_menu/fox_title.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/libc_math64.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/sys/sys_math.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/sys/sys_memory.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/assets/yaml/us/rev1/ast_title.yaml" ]; then \
		printf '%s\n' "error: original SF64 source not found under $(SF64_SOURCE_DIR)."; \
		printf '%s\n' "set SF64_SOURCE_DIR to the local sf64-psp source checkout."; \
		exit 2; \
	fi

$(PS3_BUILD_DIR)/n64ps3/src/backends/ps3/rdprsx.o: $(PS3_TRIANGLE_SHADER_HEADERS)

$(PS3_TRIANGLE_VPO): $(N64PS3_PS3_SHADER_DIR)/rdprsx_triangle.vcg
	@mkdir -p $(dir $@)
	LD_LIBRARY_PATH='$(CGCOMP_LD_LIBRARY_PATH)' $(CGCOMP) -v $< $@

$(PS3_TRIANGLE_FPO): $(N64PS3_PS3_SHADER_DIR)/rdprsx_triangle.fcg
	@mkdir -p $(dir $@)
	LD_LIBRARY_PATH='$(CGCOMP_LD_LIBRARY_PATH)' $(CGCOMP) -f $< $@

$(PS3_TRIANGLE_VPO_HEADER): $(PS3_TRIANGLE_VPO) tools/embed_binary.py
	$(PYTHON) tools/embed_binary.py $< $@ rdprsx_triangle_vpo

$(PS3_TRIANGLE_FPO_HEADER): $(PS3_TRIANGLE_FPO) tools/embed_binary.py
	$(PYTHON) tools/embed_binary.py $< $@ rdprsx_triangle_fpo

$(PS3_SELF_INPUT_ELF): $(PS3_TARGET_ELF)
	@mkdir -p $(dir $@)
	$(PPU_STRIP) $< -o $@
	$(SPRX_LINKER) $@

$(PS3_TARGET_SELF): $(PS3_SELF_INPUT_ELF)
	$(MAKE_SELF) $< $@

$(PS3_TARGET_FAKE_SELF): $(PS3_TARGET_ELF)
	$(FSELF) $< $@

$(PS3_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(PPU_CC) $(PS3_CPPFLAGS) $(PS3_CFLAGS) -c -o $@ $<

ifeq ($(SF64_PS3_ARWING),1)
$(PS3_TITLE_STARFIELD_OBJECT): src/sf64_title_starfield.c | check-original-title-source
	@mkdir -p $(dir $@)
	$(PPU_CC) $(PS3_CPPFLAGS) $(PS3_ORIGINAL_TITLE_CPPFLAGS) $(PS3_CFLAGS) -w -c -o $@ $<

$(PS3_ORIGINAL_STARFIELD_C): $(PS3_ORIGINAL_TITLE_SOURCE_FILES) tools/extract_title_starfield_source.py | check-original-title-source
	@mkdir -p $(dir $@)
	$(PYTHON) tools/extract_title_starfield_source.py --sf64-source-dir $(SF64_SOURCE_DIR) --output $@

$(PS3_ORIGINAL_STARFIELD_OBJECT): $(PS3_ORIGINAL_STARFIELD_C)
	$(PPU_CC) $(PS3_ORIGINAL_TITLE_CPPFLAGS) -D__cosf=cosf -D__sinf=sinf $(PS3_CFLAGS) -w -c -o $@ $<

$(PS3_ORIGINAL_TITLE_LOGIC_C): $(SF64_SOURCE_DIR)/src/overlays/ovl_menu/fox_title.c \
		$(SF64_SOURCE_DIR)/src/engine/fox_std_lib.c \
		$(SF64_SOURCE_DIR)/src/sys/sys_math.c \
		$(SF64_SOURCE_DIR)/assets/yaml/us/rev1/ast_title.yaml \
		tools/extract_title_logic_source.py tools/extract_title_starfield_source.py | check-original-title-source
	@mkdir -p $(dir $@)
	$(PYTHON) tools/extract_title_logic_source.py --sf64-source-dir $(SF64_SOURCE_DIR) --output $@

$(PS3_ORIGINAL_TITLE_LOGIC_OBJECT): $(PS3_ORIGINAL_TITLE_LOGIC_C)
	$(PPU_CC) $(PS3_ORIGINAL_TITLE_CPPFLAGS) -DTARGET_PSP $(PS3_CFLAGS) -w -c -o $@ $<

$(PS3_TITLE_RUNTIME_OBJECT): src/sf64_title_runtime.c include/sf64_ps3/title_runtime.h | check-original-title-source
	$(PPU_CC) $(PS3_CPPFLAGS) $(PS3_ORIGINAL_TITLE_CPPFLAGS) -DTARGET_PSP $(PS3_CFLAGS) -w -c -o $@ $<
endif

ifeq ($(SF64_PS3_ARWING),1)
$(PS3_ARWING_RESOURCE_C): $(SF64_PS3_ARWING_ROM) tools/extract_arwing_bank.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/extract_arwing_bank.py $< $@

$(PS3_ARWING_RESOURCE_OBJECT): $(PS3_ARWING_RESOURCE_C)
	$(PPU_CC) $(PS3_CPPFLAGS) $(PS3_CFLAGS) -c -o $@ $<

$(PS3_TITLE_RESOURCE_C): $(SF64_PS3_ARWING_ROM) tools/extract_title_bank.py
	@mkdir -p $(dir $@)
	$(PYTHON) tools/extract_title_bank.py $< $@

$(PS3_TITLE_RESOURCE_OBJECT): $(PS3_TITLE_RESOURCE_C)
	$(PPU_CC) $(PS3_CPPFLAGS) $(PS3_CFLAGS) -c -o $@ $<
endif

$(PS3_BUILD_DIR)/n64ps3/%.o: $(N64PS3_DIR)/%.c
	@mkdir -p $(dir $@)
	$(PPU_CC) $(PS3_CPPFLAGS) $(PS3_CFLAGS) -c -o $@ $<
