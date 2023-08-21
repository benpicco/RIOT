APPLICATION = hip_demo

# BOARD ?= native
BOARD ?= hip-badge
RIOTBASE ?= $(CURDIR)/../../../..

USEMODULE += random
USEMODULE += ws281x
USEMODULE += xtimer
USEMODULE += ztimer_msec

FEATURES_OPTIONAL += periph_gpio_irq

include $(RIOTBASE)/Makefile.include
