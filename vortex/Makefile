
X = $(wildcard *.c)
E = $(X:.c=)

build: $E

LDLIBS = -g $(shell pkg-config --libs vortex)
CFLAGS = -g $(shell pkg-config --cflags vortex) -Wall
CXXFLAGS = $(CFLAGS)

clean:
	rm -f $E

