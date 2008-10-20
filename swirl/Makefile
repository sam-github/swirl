# Makefile

X = $(wildcard *.c)
H = $(wildcard *.h)
O = $(X:.c=.o)

L=swirl.so

build: $L

test: build
	lua test.lua

LDFLAGS= -g
CFLAGS+= -g -Wall -Werror

# beepcore-c flags
LDFLAGS+=-L../beepcore-c/core/generic
LDLIBS+=-lbeepcore
CFLAGS+=-I../beepcore-c/core/generic

# Location of lua headers
CFLAGS+=-Ilocal

# Building a .so on OS X.
MACOSX_DEPLOYMENT_TARGET=10.3
export MACOSX_DEPLOYMENT_TARGET

CFLAGS+=-fno-common
LDFLAGS+=-bundle -undefined dynamic_lookup
LDFLAGS+=-bind_at_load

$O: $H

$L: $O ../beepcore-c/core/generic/libbeepcore.a
	gcc $(LDFLAGS) -o $@ $^ $(LDLIBS)

