include make/common.mk

N64PS3_HOST_MK := $(N64PS3_DIR)/make/n64ps3_host.mk
ifneq ($(wildcard $(N64PS3_HOST_MK)),)
include $(N64PS3_DIR)/make/n64ps3_host.mk
endif

HOST_CC ?= cc
HOST_BUILD_DIR := $(BUILD_ROOT)/host
HOST_TARGET := $(HOST_BUILD_DIR)/$(SF64_HOST_TARGET)
HOST_OBJECTS := $(patsubst src/%.c,$(HOST_BUILD_DIR)/%.o,$(SF64_COMMON_C_FILES))

HOST_N64PS3_OBJECTS := $(patsubst $(N64PS3_DIR)/%.c,$(HOST_BUILD_DIR)/n64ps3/%.o,$(N64PS3_C_FILES))

HOST_CPPFLAGS := \
	$(addprefix -I,$(SF64_INCLUDE_DIRS)) \
	$(N64PS3_HOST_CPPFLAGS) \
	-DSF64_PS3_HOST=1

HOST_CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -g
HOST_LDFLAGS ?=
N64PS3_LDLIBS ?=

.PHONY: all check-n64ps3

all: check-n64ps3 $(HOST_TARGET)

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

$(HOST_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -c -o $@ $<

$(HOST_BUILD_DIR)/n64ps3/%.o: $(N64PS3_DIR)/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -c -o $@ $<
