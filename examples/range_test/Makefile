# name of your application
APPLICATION = range_test

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# use the default network interface for the board
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif

USEMODULE += gnrc_ipv6_default
USEMODULE += gnrc_udp

USEMODULE += gnrc_icmpv6_echo

# USEMODULE += periph_uart_nonblocking

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

USEMODULE += periph_rtc

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-channel.inc.mk
