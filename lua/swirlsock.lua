-- Swirl - an implementation of BEEP for Lua
-- Copyright (c) 2008 Sam Roberts

require"sockext"
require"swirl"

--[[-
** swirlsock - extends the swirl module to allow using the socket module

Calling swirl.connect() and swirl.listen() sets up the swirl.loop to run BEEP
protocol kernels. They can be called multiple times, if you want to initiate
multiple connections, or listen on multiple ports.

Once set up, start them running by calling swirl.loop.start().

Note that swirl.loop is a shorthand for sockext.loop, see the sockext module
documentation for details.
]]
swirl.loop = sockext.loop

local loop = sockext.loop

local map = {}

local function disconnected(client, core, emsg)
    map[client] = nil
    loop.receive(client)
    loop.send(client)
    core:_cb("on_disconnected", core, emsg)
end

local function send(client)
  local core = map[client]
  local data = core:pull()

  if not data then
    loop.send(client)
    return
  end

  local sz, emsg = sockext.write(client, data)

  if sz then
    core:pulled(sz)
    core:_cb("trace_send", core, data, sz)
  elseif emsg == "timeout" then
    -- Select thought we could write data, but when we tried, the kernel didn't
    -- accept any? Well, it might happen, lets ignore.
  else
    disconnected(client, core, emsg)
  end
end

local function receive(client)
  local core = map[client]
  local data, emsg = sockext.read(client, 2*4096) -- this is a pretty arbitrary size! :-(
  if data then
    core:_cb("trace_receive", core, data)
    local ok, emsg = core:push(data)

    -- FIXME - push needs to call an on_error, or something!
    if not ok then
      local eno, emsg, echan = core:status()
      print("ERROR", eno, emsg, echan)
    end

  elseif emsg == "timeout" then
    -- ignore
  else
    disconnected(client, core, emsg)
  end
end

local function start(client, arg)
  assert(client:settimeout(0))
  assert(client:setoption("tcp-nodelay", true))

  -- get pullable notifications (core is always pushable, so don't bother with those)
  function arg.on_pullable(core)
    loop.send(core._client, send)
  end

  local core = swirl.core(arg)

  core._client = client
  map[client] = core

  loop.receive(client, receive)
  loop.send(client, send)

  return core
end

local function accept(server, arg)
  local client = server:accept()
  if not client then return end

  if  not arg.on_accept(client) then
    -- FIXME - we should actually create core with error=... set to indicate
    -- to the initiator why it's connection is being refused, and then do
    -- a clean shutdown
    client:close()
    return
  end

  start(client, arg)
end

--[[-
-- server = swirl.listen(argt, port, host)

Listen on host and port (host is optional, and defaults to any, "*").

When accepting client connections, calls arg.on_accept(client) with the
accepted client socket.

If on_accept doesn't exist or returns true, argt is passed to swirl.core() to
create a core for the client socket.

The core will call on_connected() when the BEEP connection has been established,
at which point channels can be started.

argt.il will be set to "L", listener.

Returns server socket on succes, or nil and an error message on failure.
]]
function swirl.listen(arg, port, host)
  if not arg.on_accept then
    arg.on_accept = function () return true end
  end

  arg.il = "L"

  local server, emsg = socket.bind(host or "*", port)

  if not server then
    return nil, emsg
  end

  assert(server:settimeout(0))

  loop.receive(server, function (server) accept(server, arg) end)

  return server
end

--[[-
-- client = swirl.connect(argt, port, host)

Connect to host and port (host is optional, and defaults to "localhost"), and
pass argt to swirl.core() to create a core for the client socket.

The core will call on_connected() when the BEEP connection has been established
(or on_connect_fail() if it the connection was rejected), at which point
channels can be started.

argt.il will be set to "I", initiator.

Returns client socket on success, or nil and an error message on failure.
]]
function swirl.connect(arg, port, host)
  arg.il = "I"

  local client, emsg = socket.connect(host or "127.0.0.1", port)

  if not client then
    return nil, emsg
  end

  start(client, arg)

  return client
end

