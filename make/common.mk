BUILD_ROOT := build

SF64_PROJECT := sf64-ps3
SF64_HOST_TARGET := sf64_ps3_smoke
SF64_PS3_TARGET := sf64_ps3_smoke

SF64_INCLUDE_DIRS := include

SF64_COMMON_C_FILES := \
	src/main.c \
	src/sf64_arwing.c \
	src/sf64_title_ui.c \
	src/sf64_platform_n64ps3.c \
	src/sf64_stub_game.c

N64PS3_DIR ?= extern/n64ps3
