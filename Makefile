include make/common.mk

.PHONY: host arwing-probe ps3 ps3-self ps3-fake-self ps3-arwing-self clean

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

clean:
	rm -rf $(BUILD_ROOT)
