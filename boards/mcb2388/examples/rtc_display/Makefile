APPLICATION = rtc_display

BOARD ?= mcb2388

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../..

USEMODULE += hd44780

FEATURES_REQUIRED += periph_rtc
FEATURES_REQUIRED += periph_adc

FEATURES_REQUIRED += periph_gpio_irq

# modules needed for sound
FEATURES_REQUIRED += periph_dac
FEATURES_REQUIRED += periph_timer
BLOBS += hello.raw

USEMODULE += xtimer

include $(RIOTBASE)/Makefile.include
