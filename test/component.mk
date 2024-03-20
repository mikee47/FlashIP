COMPONENT_SOC := host
COMPONENT_DEPENDS := FlashIP LittleFS Spiffs
HWCONFIG := fip_test
DISABLE_NETWORK := 1
GLOBAL_CFLAGS += -DFLASHIP_RETURN=1

.PHONY: execute
execute: flash run
