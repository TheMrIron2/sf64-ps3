include make/common.mk

HOST_CC ?= cc
HOST_BUILD_DIR := $(BUILD_ROOT)/host
HOST_TARGET := $(HOST_BUILD_DIR)/$(SF64_HOST_TARGET)
HOST_OBJECTS := $(patsubst src/%.c,$(HOST_BUILD_DIR)/%.o,$(SF64_COMMON_C_FILES))

HOST_CPPFLAGS := \
	$(addprefix -I,$(SF64_INCLUDE_DIRS)) \
	-I$(N64PS3_DIR)/include \
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

$(HOST_TARGET): $(HOST_OBJECTS)
	$(HOST_CC) $(HOST_LDFLAGS) -o $@ $^ $(N64PS3_LDLIBS)

$(HOST_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(HOST_CC) $(HOST_CPPFLAGS) $(HOST_CFLAGS) -c -o $@ $<
