# name of your application
APPLICATION = vfs_auto_mount

# If no BOARD is found in the environment, use this default:
BOARD ?= native


# Blacklisting msp430-based boards, as file syscalls are not supported
FEATURES_BLACKLIST += arch_msp430

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Modules to include:
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

# Use VFS
USEMODULE += vfs_default

# use SD card
USEMODULE += mtd_sdcard_auto_vfs

CFLAGS += -DSDCARD_SPI_PARAM_SPI="SPI_DEV(1)" -DSDCARD_SPI_PARAM_CS="GPIO_PIN(2,6)"      \
          -DSDCARD_SPI_PARAM_CLK="GPIO_PIN(2,5)" -DSDCARD_SPI_PARAM_MOSI="GPIO_PIN(2,4)" \
          -DSDCARD_SPI_PARAM_MISO="GPIO_PIN(2,7)"

include $(RIOTBASE)/Makefile.include
