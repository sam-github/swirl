-- Swirl - an implementation of BEEP for Lua
-- Copyright (c) 2008 Sam Roberts

local socket = require"socket"
local table = table

--[[-
** sockext

Socket extensions.
]]
module("sockext", package.seeall) -- TODO remove package.seeall

--[[-
Return an object that can be used with socket.select() to select on an
arbitrary file descriptor.
]]
function selectable(fd)
  fd = assert(tonumber(fd), "fd must be a number")
  local s = { fd = fd }
  function s:getfd() return fd end
  function s:dirty() return false end
  return s
end


--[[-
- data, emsg, partial = client:receive(pattern[, prefix])
- data, emsg, partial = client:receive("*f"[, size])

If pattern is not "*f", is identical to client:receive().

Otherwise, return as much data as is available, up to size bytes.  If size is
not specified, it defaults to 4096. This is useful for dealing with a TCP
stream without knowing the protocol. I.e., transparent proxying of TCP data as
it arrives.

This pattern requires setting client's timeout to 0, for non-blocking behaviour.
]]
function receive(client, pattern, prefix)
  if pattern ~= "*f" then
    return client:receive(pattern, prefix)
  end

  local size = assert(tonumber(prefix or 4096))
  local data, emsg, partial = client:receive(size)
  if  not data and partial ~= "" then
    return partial
  else
    return data, emsg
  end
end

local qrd = {_event = "receive"}

local qwr = {_event = "send"}

local looping = nil

--[[-
** sockext.loop

Runs a select loop on a set of descriptors (sockets or socket descriptors), allowing
easy managment of send ready, receive ready, and timeout events.

Events are associated with callbacks. If the callback is nil, then the
registration is cancelled.
]]
loop = {}

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

function loop.receive(o, fn)
  ins(qrd, o, fn)
end

function loop.send(o, fn)
  ins(qwr, o, fn)
end

function loop.timeout(wait, fn)
  assert(nil, "not implemented")
end

-- Debug select loop:
local q, array

if loop._debug then
  require"quote"

  q = quote.quote

  array = function(t)
    return {unpack(t)}
  end
end

function loop.start()
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
      print("select a", "r="..q(array(qrd)), "w="..q(array(qwr)))
    end

    local function call(events, actions)
      for i,sock in ipairs(events) do
	-- print(actions._event, q(sock), q(actions[sock]))
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

