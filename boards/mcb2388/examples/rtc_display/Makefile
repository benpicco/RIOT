APPLICATION = rtc_display

BOARD ?= mcb2388

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../..

# modules required for the RTC display
FEATURES_REQUIRED += periph_rtc
FEATURES_REQUIRED += periph_adc
USEMODULE += hd44780

# modules needed for sound
FEATURES_REQUIRED += periph_dac
FEATURES_REQUIRED += periph_timer
BLOBS += hello.raw

# modules needed for the shell
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps
USEMODULE += i2c_scan
USEMODULE += saul_default

# external sensors
#USEMODULE += adxl345
USEMODULE += itg320x

# modules needed for networking
USEMODULE += at86rf215
USEMODULE += auto_init_gnrc_netif
USEMODULE += gnrc_ipv6_default
USEMODULE += gnrc_udp
USEMODULE += gnrc_icmpv6_echo
USEMODULE += sock_dns

USEMODULE += xtimer

include $(RIOTBASE)/Makefile.include
