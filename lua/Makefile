# Makefile

X = $(wildcard v*.c)
H = $(wildcard v*.h)
O = $(X:.c=.o)

L=vortex.so

do: build README.API chat-client.c
	$(MAKE) -C ../c

chat-client.c: ../c/chat-client.c
	cp -v $< .

README.API: vmain.c
	luadoc vmain.c > README.API

run:
	./local/lua lchatd.lua

build: $L

LDFLAGS= -g
CFLAGS+= -g -Wall -Werror
CFLAGS+=-DENABLE_VORTEX_LOG

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
LDFLAGS+=-bind_at_load

$O: $H

$L: $O
	gcc $(LDFLAGS) -o $@ $^ $(LDLIBS)

