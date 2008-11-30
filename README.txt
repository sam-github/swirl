Swirl - BEEP for Lua, a binding of beepcore-c


* About

BEEP is a "protocol kernel" useful for implementing application protocols.

This is a binding of beepcore-c into Lua 5.1, wrapped in a more user-friendly
API implemented directly in Lua.

Swirl has no dependency on any particular TCP APIs. It can be used with any,
including the non-blocking event-based APIs common in UIs, and other
development frameworks.

However, as an example of usage, and because its useful for me, an event loop
based implementation on top of LuaSocket is included, along with some LuaSocket
extensions to select on arbitary file descriptors, and read raw data from
sockets.


* Availability

Package:

  http://luaforge.net/projects/swirl/

Source:

  http://github.com/sam-github/swirl/tree/master


* License

MIT style, the same as Lua.


* Plans

My goals are to confirm that beepcore-c works well, and is easy to bind into
another language. I started with Lua because its simple (and fun), but I don't
have any active projects that need BEEP for Lua.

I hope to take what I learn and produce a binding into Python's Twisted network
stack, next, and if I ever need a BEEP library for Ruby, I'll know how to make
one easily.


* Benchmarking

One concern of mine with BEEP implementations I've worked with is how fast they
can transfer data. Swirl includes some benchmarks to compare performance with
raw TCP (bm-beep-client/server and bm-raw-client/server).

The benchmark profile is also implemented for Vortex, and I will try to do one
for beep4j as well.  I'm curious to see how the toolkits compare, and I suspect
that techniques to maximize performance for one will work for all of them.


* Beepcore-c

Since beepcore-c is an abandoned project, it might seem odd that I should use
it to implement Swirl. I chose it because its architecture seems to be designed
for ease of embedding in a host language. It's architecture is described here:

http://beepcore-c.sourceforge.net/Architecture.html

Swirl discards the multi-threaded, overly complex, and overly ambitious wrapper
portions of the library, and is implemented directly on the "core".

A bug-fixed copy of the beepcore-c core code is included in Swirl, and is
statically linked into swirl.so.


* Vortex

A previous attempt at Swirl had attempted to bind Vortex into Lua. However, the
pervasive multi-threaded nature of Vortex proved to difficult to deal with. I
had to attempt to make Vortex look single-threaded to the Lua interpreter. This
was quite difficult because Vortex is heavily callback based, but the callbacks
happen on unpredictable threads.

My deadlocks might have been solveable, or might not, but I began to feel I was
fighting uphill. Remnants of this early attempt exist in the code base, such as
the on_ convention for callbacks, and I really appreciate Francis Blazquez's
friendly support while trying to use his toolkit.


* References

http://beepcore-c.sourceforge.net/
http://lua.org
http://www.aspl.es/vortex/
http://www.beepcore.org/
http://www.tecgraf.puc-rio.br/luasocket/


* Contact

Please direct suggestions, patches, offers to sponsor continued development,
etc., to Sam Roberts <vieuxtech@gmail.com>


* Manifest

Swirl is built by the top-level Makefile.

./config

  will need to be modified to customize the build, at least to choose a platform

./beepcore-c

  beepcore C library (only some of it is used by Swirl)
   
    COPYRIGHT - the "Blocks Public License" for beepcore-c
    core/generic/ - the beepcore-c "core"
    utility/ - non-BEEP code that the "core" uses

./lua/

  Swirl's lua binding, contains:

    API.txt - description of the API
    USAGE.txt - notes on API usage
    COPYRIGHT.txt - copyright for Swirl

    ex-chatd/ex-chat - example chat server and client
    bm-beep-server/client - example BEEP null benchmark server and client
    bm-raw-server/client - example TCP null benchmark server and client
    *-test - tests

    swirl.c - binding to beepcore-c
    swirl.lua - extends the binding to be useful
    swirlsock.lua - extends the core to work with luasocket, also serves
      as an example of how to use the core if you wish to use another TCP
      transport API (other than luasocket)
    sockext.lua - luasocket event loop used by swirlsock
  

Unpackaged code, not part of Swirl, but in the source repository:

./qt

  a chat client, written with Qt and vortex (an exercise in using
  multi-threaded vortex in single-threaded Qt)

./vortex

  null benchmark client and server implemented with vortex

