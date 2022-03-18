PKG_NAME=storfs
PKG_URL=https://github.com/matt001k/STORfs.git
PKG_VERSION=31a83a426057e8fca5c5c1122e99f9f495102763
PKG_LICENSE=Apache-2.0

include $(RIOTBASE)/pkg/pkg.mk

CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-unused-variable
CFLAGS += -Wno-sign-compare
CFLAGS += -Wno-format

all:
	$(QQ)"$(MAKE)" -C $(PKG_SOURCE_DIR)/src -f $(RIOTBASE)/Makefile.base MODULE=storfs
