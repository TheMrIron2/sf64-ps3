include make/common.mk

.PHONY: host arwing-probe ps3 ps3-self ps3-fake-self ps3-arwing-self ps3-arwing-nodepth-self ps3-arwing-static-self clean

host:
	$(MAKE) -f make/host.mk

arwing-probe:
	$(MAKE) -f make/host.mk arwing-probe

ps3:
	$(MAKE) -f make/ps3.mk

ps3-self:
	$(MAKE) -f make/ps3.mk self

ps3-fake-self:
	$(MAKE) -f make/ps3.mk fake-self

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
