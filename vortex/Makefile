
X = $(wildcard *.c)
E = $(X:.c=)

build: $E

#LDLIBS = -g $(shell pkg-config --libs vortex)
#CFLAGS = -g $(shell pkg-config --cflags vortex) -Wall

#CFLAGS+=-I/Users/sam/p/beep/vortex/svn-vortex/src -I/Users/sam/p/beep/vortex/svn-axl/src
#LDLIBS+=/Users/sam/p/beep/vortex/svn-vortex/src/libvortex.a /Users/sam/p/beep/vortex/svn-axl/src/libaxl.a

CFLAGS+=-I/usr/local/include/vortex -I/usr/local/include/axl
LDLIBS+=-lvortex -laxl

CXXFLAGS = $(CFLAGS)

clean:
	rm -f $E

