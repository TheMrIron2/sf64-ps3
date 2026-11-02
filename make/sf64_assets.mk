SF64_ASSET_METADATA := $(wildcard $(SF64_SOURCE_DIR)/assets/yaml/us/rev1/*.yaml) \
	$(wildcard $(SF64_SOURCE_DIR)/assets/yaml/us/rev1/*.yml)
SF64_ASSET_ENUM_HEADERS := $(wildcard $(SF64_SOURCE_DIR)/include/*.h)

$(SF64_ASSET_STAMP): $(SF64_ASSET_ROM) $(SF64_SOURCE_DIR)/config.yml \
		$(SF64_ASSET_METADATA) $(SF64_ASSET_ENUM_HEADERS) \
		$(SF64_SOURCE_DIR)/tools/comptool.py \
		$(SF64_SOURCE_DIR)/tools/mio0-decompressor/libmio0.c \
		tools/generate_sf64_headers.py
	$(PYTHON) tools/generate_sf64_headers.py --source "$(SF64_SOURCE_DIR)" \
		--rom "$(SF64_ASSET_ROM)" --torch "$(SF64_TORCH)" \
		--output "$(SF64_ASSET_DIR)" --cc "$(HOST_CC)"
	@touch "$@"
