-- Swirl - an implementation of BEEP for Lua
-- Copyright (c) 2008 Sam Roberts

local socket = require"socket"
local table = table

--[[-
** sockext

Extensions to "socket", the LuaSocket library.

]]
module("sockext", package.seeall) -- TODO remove package.seeall

--[[-
-- selectable = sockext.selectable(fd)

Return an object that can be used with socket.select() to select on a
arbitrary descriptor.

fd is an integer descriptor.
]]
function selectable(fd)
  fd = assert(tonumber(fd), "fd must be a number")
  local s = { fd = fd }
  function s:getfd() return fd end
  function s:dirty() return false end
  return s
end

--[[-
-- data, emsg = sockext.read(client, pattern[, prefix])

Identical to client:receive(), except that if partial data was read, it returns
it instead of returning nil.

This is convenient for binary/non-line-delimited protocols.
]]
function read(client, pattern, prefix)
  local data, emsg, partial = client:receive(pattern, prefix)
  if data then
    return data
  end
  if partial and #partial > 0 then
    return partial
  end
  return nil, emsg
end

--[[-
-- last, emsg = sockext.write(client, data, i, j)

Identical to client:send(), except that if partial data was written, it returns
the index of the last byte of the partial write, instead of returning nil.

In the common case that i is nil, this means that the return value is always
the amount of data sent, even for partial sends.
]]
function write(client, data, i, j)
  local last1, emsg, last2 = client:send(data, i, j)

  if last1 then
    return last1
  end

  -- the luasocket docs aren't clear about what last2 will be if no data was
  -- written, I hope it will be either nil, or a number less than i
  if emsg == "timeout" and last2 and last2 >= (i or 1) then
    return last2
  end

  return nil, emsg
end


local qrd = {_event = "receive"}

local qwr = {_event = "send"}

local looping = nil

--[[-
** sockext.loop

Implementation of a select loop on a set of descriptors (sockets or socket
descriptors), allowing cooperative managment of send ready and receive ready
event handlers.

Events are associated with callbacks. If the callback is nil, then the
registration is cancelled.
]]
loop = {}

-- Debug select loop:
local q, array

--[[-
-- sockext.loop.start()

Loops, selects on all the send and receive selectables, calling handlers.

Asserts if no handlers are registered.

If during looping, all event handlers are cleared, returns.

If during looping, loop.stop() is called, returns.
]]
function loop.start()
  if loop._debug then
    require"quote"

    q = quote.quote

    array = function(t)
      return {unpack(t)}
    end
  end

  assert((#qrd + #qwr) > 0, "Nothing to loop for")
  looping = true
  while looping do
    if loop._debug then
      print("select q", "r="..q(array(qrd)), "w="..q(array(qwr)))
    end

    if #qrd + #qwr == 0 then
      -- don't block forever on empty socket lists
      looping = false
      break
    end

    local ard, awr, err = socket.select(qrd, qwr)

    if loop._debug then
      print("select a", "r="..q(array(ard)), "w="..q(array(awr)), err)
    end

    local function call(events, actions)
      for i,sock in ipairs(events) do
	if loop._debug then
	  print(actions._event, q(sock), q(actions[sock]))
	end
	local action = actions[sock]
	-- earlier actions may have deregistered for events
	if action then
	  action(sock)
	end
      end
    end

    call(awr, qwr)
    call(ard, qrd)
  end
end

--[[-
-- sockext.loop.stop()

When called within an event handler, causes the event loop to stop.
]]
function loop.stop()
  looping = false
end

local function ins(t, o, fn)
  assert(o, "o is nil")
  assert(o:getfd(), tostring(o))

  -- remove existing
  for i,v in ipairs(t) do
    if v == o then
      table.remove(t, i)
      t[o] = nil
    end
  end
  if fn then
    -- if fn, re-add to t
    table.insert(t, o)
    t[o] = fn
  end
end

--[[-
-- sockext.loop.receive(selectable[, handler])

Set handler to be called when selectable is receive-ready.

Only one receive handler can be handled per selectable.

Unsets the handler if handler is nil.
]]
function loop.receive(o, fn)
  ins(qrd, o, fn)
end

--[[-
-- sockext.loop.send(selectable[, handler])

Set handler to be called when selectable is send-ready.

Only one send handler can be handled per selectable.

Unsets the handler if handler is nil.
]]
function loop.send(o, fn)
  ins(qwr, o, fn)
end

function loop.timeout(wait, fn)
  assert(nil, "not implemented")
end

