include make/common.mk

.DEFAULT_GOAL := all

N64PS3_HOST_MK := $(N64PS3_DIR)/make/n64ps3_host.mk
ifneq ($(wildcard $(N64PS3_HOST_MK)),)
include $(N64PS3_DIR)/make/n64ps3_host.mk
endif

HOST_CC ?= cc
PYTHON ?= python3
HOST_BUILD_DIR := $(BUILD_ROOT)/host
HOST_TARGET := $(HOST_BUILD_DIR)/$(SF64_HOST_TARGET)
HOST_ARWING_PROBE_TARGET := $(HOST_BUILD_DIR)/sf64_ps3_arwing_probe
HOST_ARWING_OBJECT := $(HOST_BUILD_DIR)/sf64_arwing.o
HOST_TITLE_STARFIELD_OBJECT := $(HOST_BUILD_DIR)/sf64_title_starfield.o
HOST_ORIGINAL_STARFIELD_C := $(HOST_BUILD_DIR)/original_title_starfield.c
HOST_ORIGINAL_STARFIELD_OBJECT := $(HOST_BUILD_DIR)/original_title_starfield.o
SF64_SOURCE_DIR ?= ../sf64-psp
HOST_ORIGINAL_TITLE_SOURCE_FILES := \
	$(SF64_SOURCE_DIR)/src/engine/fox_bg.c \
	$(SF64_SOURCE_DIR)/src/engine/fox_play.c \
	$(SF64_SOURCE_DIR)/src/libc_math64.c \
	$(SF64_SOURCE_DIR)/src/sys/sys_math.c \
	$(SF64_SOURCE_DIR)/src/sys/sys_memory.c
HOST_ORIGINAL_TITLE_CPPFLAGS := \
	-I$(SF64_SOURCE_DIR)/include \
	-I$(SF64_SOURCE_DIR) \
	-I$(SF64_SOURCE_DIR)/src \
	-D_LANGUAGE_C -D_MIPS_SZLONG=32
HOST_ORIGINAL_TITLE_CFLAGS ?= -std=gnu99 -O2 -ffunction-sections -fdata-sections -w
HOST_OBJECTS := $(patsubst src/%.c,$(HOST_BUILD_DIR)/%.o,$(SF64_COMMON_C_FILES))

HOST_N64PS3_OBJECTS := $(patsubst $(N64PS3_DIR)/%.c,$(HOST_BUILD_DIR)/n64ps3/%.o,$(N64PS3_C_FILES))
HOST_DEPENDENCIES := $(HOST_OBJECTS:.o=.d) $(HOST_N64PS3_OBJECTS:.o=.d)
HOST_DEPENDENCIES += $(HOST_TITLE_STARFIELD_OBJECT:.o=.d) $(HOST_ORIGINAL_STARFIELD_OBJECT:.o=.d)

HOST_CPPFLAGS := \
	$(addprefix -I,$(SF64_INCLUDE_DIRS)) \
	$(N64PS3_HOST_CPPFLAGS) \
	-DSF64_PS3_HOST=1

HOST_CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -g
HOST_CFLAGS += -MMD -MP
HOST_LDFLAGS ?=
N64PS3_LDLIBS ?= -lm

-include $(HOST_DEPENDENCIES)

.PHONY: all arwing-probe check-n64ps3 check-original-title-source

all: check-n64ps3 $(HOST_TARGET)

arwing-probe: check-n64ps3 $(HOST_ARWING_PROBE_TARGET)

check-n64ps3:
	@if [ ! -d "$(N64PS3_DIR)" ]; then \
		printf '%s\n' "error: $(N64PS3_DIR) is missing."; \
		printf '%s\n' "setup: git submodule add https://github.com/TheMrIron2/n64ps3 $(N64PS3_DIR)"; \
		exit 2; \
	fi
	@if [ ! -d "$(N64PS3_DIR)/include" ]; then \
		printf '%s\n' "error: $(N64PS3_DIR) exists but has no include/ directory."; \
		printf '%s\n' "n64ps3 is not buildable/consumable by this smoke target yet."; \
		exit 2; \
	fi
	@if [ ! -f "$(N64PS3_HOST_MK)" ]; then \
		printf '%s\n' "error: $(N64PS3_HOST_MK) is missing."; \
		printf '%s\n' "update extern/n64ps3 to a revision with Makefile exports."; \
		exit 2; \
	fi

$(HOST_TARGET): $(HOST_OBJECTS) $(HOST_N64PS3_OBJECTS)
	$(HOST_CC) $(HOST_LDFLAGS) -o $@ $^ $(N64PS3_LDLIBS)

$(HOST_ARWING_PROBE_TARGET): tools/arwing_probe.c $(HOST_ARWING_OBJECT) $(HOST_TITLE_STARFIELD_OBJECT) $(HOST_ORIGINAL_STARFIELD_OBJECT) $(HOST_N64PS3_OBJECTS)
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) $(HOST_LDFLAGS) -o $@ $^ $(N64PS3_LDLIBS)

check-original-title-source:
	@if [ ! -f "$(SF64_SOURCE_DIR)/src/engine/fox_bg.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/engine/fox_play.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/libc_math64.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/sys/sys_math.c" ] || \
	   [ ! -f "$(SF64_SOURCE_DIR)/src/sys/sys_memory.c" ]; then \
		printf '%s\n' "error: original SF64 source not found under $(SF64_SOURCE_DIR)."; \
		printf '%s\n' "set SF64_SOURCE_DIR to the local sf64-psp source checkout."; \
		exit 2; \
	fi

$(HOST_TITLE_STARFIELD_OBJECT): src/sf64_title_starfield.c | check-original-title-source
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_ORIGINAL_TITLE_CPPFLAGS) $(HOST_ORIGINAL_TITLE_CFLAGS) -MMD -MP -c -o $@ $<

$(HOST_ORIGINAL_STARFIELD_C): $(HOST_ORIGINAL_TITLE_SOURCE_FILES) tools/extract_title_starfield_source.py | check-original-title-source
	@mkdir -p $(dir $@)
	$(PYTHON) tools/extract_title_starfield_source.py --sf64-source-dir $(SF64_SOURCE_DIR) --output $@

$(HOST_ORIGINAL_STARFIELD_OBJECT): $(HOST_ORIGINAL_STARFIELD_C)
	$(HOST_CC) $(HOST_ORIGINAL_TITLE_CPPFLAGS) -D__cosf=cosf -D__sinf=sinf $(HOST_ORIGINAL_TITLE_CFLAGS) -MMD -MP -c -o $@ $<

$(HOST_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -c -o $@ $<

$(HOST_BUILD_DIR)/n64ps3/%.o: $(N64PS3_DIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -c -o $@ $<
