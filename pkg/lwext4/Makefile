PKG_NAME=lwext4
PKG_URL=https://github.com/RT-Thread-packages/lwext4.git
PKG_VERSION=3b7938080e80fb72bad5bb5506e6e782a544a47d
PKG_LICENSE=GPLv2

CFLAGS += -Wno-cast-align
CFLAGS += -Wno-error=format
CFLAGS += -Wno-unused-parameter -Wno-unused-variable

include $(RIOTBASE)/pkg/pkg.mk

all:
	$(QQ)"$(MAKE)" -C $(PKG_SOURCE_DIR)/src -f $(RIOTBASE)/Makefile.base MODULE=$(PKG_NAME)
