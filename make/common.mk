BUILD_ROOT := build

SF64_PROJECT := sf64-ps3
SF64_HOST_TARGET := sf64_ps3_smoke
SF64_PS3_TARGET := sf64_ps3_smoke

SF64_INCLUDE_DIRS := include

SF64_SOURCE_DIR ?= game
HOST_CC ?= cc
SF64_PS3_ARWING_ROM ?= baserom.us.rev1.z64
SF64_ASSET_ROM ?= $(SF64_PS3_ARWING_ROM)
SF64_TORCH ?= build/tools/torch/torch
SF64_ASSET_DIR := $(BUILD_ROOT)/generated/sf64
SF64_ASSET_HEADERS := $(SF64_ASSET_DIR)/generated
SF64_ASSET_STAMP := $(SF64_ASSET_DIR)/headers.stamp

SF64_COMMON_C_FILES := \
	src/main.c \
	src/sf64_display.c \
	src/sf64_controller.c \
	src/sf64_boot.c \
	src/sf64_arwing.c \
	src/sf64_title_ui.c \
	src/sf64_platform_n64ps3.c \
	src/sf64_stub_game.c

N64PS3_DIR ?= extern/n64ps3
