BOARD ?= iotlab-m3
IOTLAB_EXP_NAME := nanocoap_non_block
IOTLAB_NODES ?= 100

include ../Makefile.tests_common

# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += netdev_default
USEMODULE += auto_init_gnrc_netif
# Specify the mandatory networking modules
USEMODULE += gnrc_ipv6_default

USEMODULE += event_periodic_callback

# Instead of simulating an Ethernet connection, we can also simulate
# an IEEE 802.15.4 radio using ZEP
USE_ZEP ?= 1

# set the ZEP port for native
ZEP_PORT_BASE ?= 17754
ifeq (1,$(USE_ZEP))
  ifeq (native, $(BOARD))
    TERMFLAGS += -z [::1]:$(ZEP_PORT_BASE)
    USEMODULE += socket_zep
  endif

  USEMODULE += gnrc_ipv6_router_default
  # Add a routing protocol
  USEMODULE += gnrc_rpl
  USEMODULE += auto_init_gnrc_rpl

  ifneq (,$(ZEP_MAC))
    TERMFLAGS += --eui64=$(ZEP_MAC)
  endif
else
  USEMODULE += gnrc_ipv6_auto_subnets
  USEMODULE += gnrc_ipv6_router_default
endif

# Optionally include DNS support. This includes resolution of names at an
# upstream DNS server and the handling of RDNSS options in Router Advertisements
# to auto-configure that upstream DNS server.
# USEMODULE += sock_dns              # include DNS client
# USEMODULE += gnrc_ipv6_nib_dns     # include RDNSS option handling

USEMODULE += nanocoap_sock
USEMODULE += nanocoap_page
USEMODULE += nanocoap_page_forward

USEMODULE += gnrc_icmpv6_echo ps
CFLAGS += -DNETDEV_TAP_MAX=2

USEMODULE += nanocoap_shard_test

CFLAGS += -DCONFIG_IEEE802154_DEFAULT_CHANNEL=20

# boards where basic nanocoap functionality fits, but no VFS
LOW_MEMORY_BOARDS := \
  atmega1284p \
  atxmega-a3bu-xplained \
  blackpill-stm32f103c8 \
  bluepill-stm32f103c8 \
  derfmega128 \
  im880b \
  nucleo-f302r8 \
  saml10-xpro \
  saml11-xpro \
  sipeed-longan-nano \
  spark-core \
  stm32f7508-dk \
  stm32mp157c-dk2 \
  #

# Don't enable VFS, DTLS functions on small boards
ifeq (,$(filter $(BOARD),$(LOW_MEMORY_BOARDS)))
#  USEMODULE += nanocoap_dtls
#  USEMODULE += prng_sha256prng

  USEMODULE += nanocoap_vfs
  USEMODULE += vfs_default
  USEMODULE += vfs_auto_format
  USEMODULE += shell_cmds_default
  USEMODULE += shell_cmd_genfile

  # DTLS and VFS operations require more stack on the main thread
  CFLAGS += -DTHREAD_STACKSIZE_MAIN=\(4*THREAD_STACKSIZE_DEFAULT\)

  # always enable auto-format for native
  ifeq ($(BOARD),native)
    USEMODULE += vfs_auto_format
    CFLAGS += -DEVENT_THREAD_MEDIUM_STACKSIZE=8192
  else
    CFLAGS += -DEVENT_THREAD_MEDIUM_STACKSIZE=2048
  endif
endif

# Required by test
USEMODULE += od
USEMODULE += shell

include $(RIOTBASE)/Makefile.include
include $(RIOTBASE)/dist/testbed-support/Makefile.iotlab
