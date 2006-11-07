X = $(wildcard *.c)
O = $(X:.c=.o)

L=vortex.so

build: $L

LDLIBS= -g
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

$L: $O
	gcc $(LDFLAGS) -o $@ $^ $(LDLIBS)

