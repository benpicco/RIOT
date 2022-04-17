include ../Makefile.tests_common

# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += netdev_default
USEMODULE += auto_init_gnrc_netif
# Specify the mandatory networking modules
USEMODULE += gnrc_ipv6_default
USEMODULE += sock_udp

USEMODULE += nanocoap_sock

# boards where basic nanocoap functionality fits, but no VFS
LOW_MEMORY_BOARDS := atmega1284p atxmega-a3bu-xplained derfmega128 \
                     saml11-xpro bluepill saml10-xpro blackpill nucleo-f302r8 \
                     stm32mp157c-dk2 stm32f7508-dk

# Don't enable VFS functions on small boards
ifeq (,$(filter $(BOARD),$(LOW_MEMORY_BOARDS)))
  USEMODULE += nanocoap_vfs
  USEMODULE += vfs_default
  # USEMODULE += vfs_auto_format
  USEMODULE += shell_commands
endif

# Required by test
USEMODULE += od
USEMODULE += shell

include $(RIOTBASE)/Makefile.include
