include make/common.mk

.PHONY: host boot-probe arwing-probe option-probe controller-probe title-tools ps3 ps3-self ps3-fake-self ps3-boot-self ps3-arwing-self ps3-arwing-nodepth-self ps3-arwing-static-self clean

title-tools:
	@test -f game/tools/Torch/CMakeLists.txt || { \
		printf '%s\n' 'Initialize Torch: git submodule update --init game/tools/Torch'; exit 1; }
	cmake -S game/tools/Torch -B build/tools/torch -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -Wno-dev
	cmake --build build/tools/torch --parallel 4

host:
	$(MAKE) -f make/host.mk

boot-probe:
	$(MAKE) -f make/host.mk boot-probe

arwing-probe:
	$(MAKE) -f make/host.mk arwing-probe

controller-probe:
	$(MAKE) -f make/host.mk controller-probe

option-probe:
	$(MAKE) -f make/host.mk option-probe

ps3:
	$(MAKE) -f make/ps3.mk

ps3-self:
	$(MAKE) -f make/ps3.mk self

ps3-fake-self:
	$(MAKE) -f make/ps3.mk fake-self

ps3-boot-self:
	$(MAKE) -f make/ps3.mk self BUILD_ROOT=build/boot \
		SF64_PS3_TARGET=sf64_ps3_boot SF64_PS3_ARWING=1 SF64_PS3_BOOT=1

ps3-arwing-self:
	$(MAKE) -f make/ps3.mk self \
		BUILD_ROOT=build/arwing \
		SF64_PS3_TARGET=sf64_ps3_arwing \
		SF64_PS3_ARWING=1 \
		SF64_PS3_ARWING_ROM=baserom.us.rev1.z64

ps3-arwing-nodepth-self:
	$(MAKE) -f make/ps3.mk self \
		BUILD_ROOT=build/arwing-nodepth \
		SF64_PS3_TARGET=sf64_ps3_arwing_nodepth \
		SF64_PS3_ARWING=1 \
		SF64_PS3_ARWING_ROM=baserom.us.rev1.z64 \
		PS3_SHADER_CACHE_DIR=build/arwing/ps3/n64ps3/src/backends/ps3 \
		PS3_EXTRA_CPPFLAGS=-DN64PS3_RSX_DISABLE_DEPTH=1

ps3-arwing-static-self:
	$(MAKE) -f make/ps3.mk self \
		BUILD_ROOT=build/arwing-static \
		SF64_PS3_TARGET=sf64_ps3_arwing_static \
		SF64_PS3_ARWING=1 \
		SF64_PS3_ARWING_ROM=baserom.us.rev1.z64 \
		PS3_SHADER_CACHE_DIR=build/arwing/ps3/n64ps3/src/backends/ps3 \
		PS3_EXTRA_CPPFLAGS="-DSF64_PS3_STATIC_ARWING_POSE=1 -DN64PS3_RSX_TEXTURE_DIAGNOSTICS=1"

clean:
	rm -rf $(BUILD_ROOT)
