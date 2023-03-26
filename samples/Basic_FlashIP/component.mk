COMPONENT_SOC := \
	host \
	rp2040

COMPONENT_DEPENDS := \
	FlashIP \
	LittleFS

HWCONFIG := basic_flaship_$(SMING_SOC)

# We do not want pico-W WiFi firmware bound up in the application image; it doesn't change
LINK_CYW43_FIRMWARE := 0

# download urls, set appropriately
CONFIG_VARS += FIRMWARE_URL

FIRMWARE_URL ?= "http://192.168.7.5:9999/app.bin"

APP_CFLAGS = -DFIRMWARE_URL="\"$(FIRMWARE_URL)"\"
