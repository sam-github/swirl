require"sockext"
require"swirl"

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

  if not data then return end

  local sz, emsg = client:send(data)

  if sz then
    core:pulled(sz)
    core:_cb("trace_send", core, data, sz)
  else
    disconnected(client, core, emsg)
  end
end

local function receive(client)
  local core = map[client]
  local data, emsg = sockext.receive(client, "*f", swirl.BUFSZ)
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

  local core = swirl.session(arg)

  core._client = client
  map[client] = core

  loop.receive(client, receive)
  loop.send(client, send)
end

local function accept(server, arg)
  local client = server:accept()
  if not client then return end

  if  not arg.on_accept(client) then
    -- FIXME - we should actually create session with error=... set to indicate
    -- to the initiator why it's connection is being refused, and then do
    -- a clean shutdown
    client:close()
    return
  end

  start(client, arg)
end

--[[-
Listen on host and port (host is optional, and defaults to any, "*").

When accepting client connections, calls arg.on_accept(client) with the
accepted client socket.

If on_accept doesn't exist or returns true, arg is passed to swirl.session() to
create a core for the client.

The core will call on_connected() when the BEEP session has been established,
at which point channels can be started.

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

  server:settimeout(0)

  loop.receive(server, function (server) accept(server, arg) end)

  return server
end

--[[-
Connect to host and port (host is optional, and defaults to "localhost")

The core will call on_connected() when the BEEP session has been established
(or on_connect_fail() if it the connection was rejected), at which point
channels can be started.
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

