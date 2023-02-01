PKG_NAME=esp32_sdk
PKG_URL=https://github.com/espressif/esp-idf
# v4.4.4
PKG_VERSION=e8bdaf91986a41678adc6be13888fc037b1acb68
PKG_LICENSE=Apache-2.0

include $(RIOTBASE)/pkg/pkg.mk

ESP32_SDK_VER_FILE = $(PKG_SOURCE_DIR)/components/esp_idf_ver.h
ESP32_SDK_VER_CMD = $(shell git -C $(PKG_SOURCE_DIR) describe --tags $(PKG_VERSION))

all: $(ESP32_SDK_VER_FILE)

$(PKG_PREPARED): $(ESP32_SDK_VER_FILE)

# Set the SDK version from the SDK hash/tag. For example "v4.4-98-geb3797dc3ff".
$(ESP32_SDK_VER_FILE): $(PKG_PATCHED)
	$(Q)echo "#define IDF_VER \"$(ESP32_SDK_VER_CMD)\"" > $@
