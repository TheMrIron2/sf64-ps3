include make/common.mk

PS3_BUILD_DIR := $(BUILD_ROOT)/ps3
PS3_TARGET_ELF := $(PS3_BUILD_DIR)/$(SF64_PS3_TARGET).elf
PS3_OBJECTS := $(patsubst src/%.c,$(PS3_BUILD_DIR)/%.o,$(SF64_COMMON_C_FILES))

PPU_CC ?= $(PS3DEV)/ppu/bin/ppu-gcc

PS3_CPPFLAGS := \
	$(addprefix -I,$(SF64_INCLUDE_DIRS)) \
	-I$(N64PS3_DIR)/include \
	-I$(PSL1GHT)/include \
	-DSF64_PS3_TARGET=1

PS3_CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O0
PS3_LDFLAGS ?= -L$(PSL1GHT)/lib
N64PS3_PS3_LDLIBS ?=
PS3_LDLIBS ?=

.PHONY: all check-ps3-env check-n64ps3

all: check-ps3-env check-n64ps3 $(PS3_TARGET_ELF)
	@printf '%s\n' "built PS3 smoke ELF: $(PS3_TARGET_ELF)"
	@printf '%s\n' "note: SELF/PKG packaging is intentionally not faked by this skeleton."

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

$(PS3_TARGET_ELF): $(PS3_OBJECTS)
	$(PPU_CC) $(PS3_LDFLAGS) -o $@ $^ $(N64PS3_PS3_LDLIBS) $(PS3_LDLIBS)

$(PS3_BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(PPU_CC) $(PS3_CPPFLAGS) $(PS3_CFLAGS) -c -o $@ $<
