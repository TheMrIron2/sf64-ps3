include make/common.mk

.PHONY: host ps3 clean

host:
	$(MAKE) -f make/host.mk

ps3:
	$(MAKE) -f make/ps3.mk

clean:
	rm -rf $(BUILD_ROOT)
