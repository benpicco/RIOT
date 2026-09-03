MODULE = codec2lite

# codec2lite ships with a main.c demo application (the reference encoder/
# decoder round-trip test) that is not part of the library and must not be
# compiled into the module.
SRC := $(filter-out main.c, $(wildcard *.c))

include $(RIOTBASE)/Makefile.base
