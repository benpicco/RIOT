# c2wave - standalone linux command line tool built on top of the
# codec2lite package sources (pkg/codec2lite), for converting between
# WAV files and Codec 2 raw bitstreams.
#
# Usage:
#   make -C dist/tools/codec2lite
#
# This expects the codec2lite package sources to have been fetched into
# $(RIOTBASE)/build/pkg/codec2lite already, e.g. by building any RIOT
# application that uses `USEPKG += codec2lite`.

RIOTBASE ?= $(CURDIR)/../../..
PKG_SOURCE_DIR ?= $(RIOTBASE)/build/pkg/codec2lite

CC ?= cc
CFLAGS += -Wall -Wextra -std=gnu11 -O2 -g
CFLAGS += -I$(PKG_SOURCE_DIR)
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -Wno-discarded-qualifiers -Wno-old-style-definition \
          -Wno-strict-prototypes -Wno-unused-parameter -Wno-cast-align
LDLIBS += -lm

LIBSRC = $(filter-out $(PKG_SOURCE_DIR)/main.c, $(wildcard $(PKG_SOURCE_DIR)/*.c))

BIN = c2wave

all: $(BIN)

$(BIN): c2wave.c $(LIBSRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

.PHONY: clean
clean:
	rm -f $(BIN)
