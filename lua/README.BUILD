# Outline of a Makefile, will need modification for local environment.

LDFLAGS= -g
CFLAGS= -g -Wall -Werror

# Vortex flags
LDLIBS+=$(shell pkg-config --libs vortex)
CFLAGS+=$(shell pkg-config --cflags vortex)

# Location of lua headers
CFLAGS+=-Ilocal

# Building a .so on OS X.
MACOSX_DEPLOYMENT_TARGET=10.3
export MACOSX_DEPLOYMENT_TARGET

CFLAGS+=-fno-common
LDFLAGS+=-bundle -undefined dynamic_lookup

vortex.so: vmain.o
	gcc $(LDFLAGS) -o $@ $^ $(LDLIBS)


chat-client: chat-client.o

