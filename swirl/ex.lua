require"swirl"

require"table2str"

local function q(t)
  return serialize(t)
end


print"++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"

-- swirl API
local mt = {}

mt.__index = mt

swirl._session_mt = mt

function mt:_cb(cb, ...)
  print("-> _cb "..cb, ...)
  -- TODO callbacks might all need to take session as the first argument,
  -- because they can't capture the session in their closure (they are passed
  -- before the session exists)
  -- TODO should pcall these. if error raised, call on_error (without pcall)
  -- (so user can choose whether to log error somewhere, or to ignore, exit,
  -- whatever)
  cb = self.arg[cb]
  if cb then
    cb(...)
  end
end

function mt:pull()
  self.lower.outready = false
  return self.core:pull()
end

function mt:push(buffer)
  self.lower.inready = nil
  self.core:push(buffer)

  -- process upper layer...
  while #self.upper > 0 do
    local chno, op = unpack(table.remove(self.upper))
    print("  upper ch#"..chno.." "..op)
    if op == "frame" or op == "message" then
      if chno == 0 then
	local chan0 = self:chan0_read()
	if not chan0 then
	  -- message could already have been consumed
	else
	  print(q(chan0))
	  local status, op = chan0:op()
	  print("op "..op.." status "..status)
	  if op == "g" then
	    if status == "c" then
	      self:_cb("on_accepted")
	    elseif status == "e" then
	      self:_cb("on_rejected") -- TODO pass the error message
	    else
	      assert(nil, ("UNHANDLED op %s, status %s"):format(op, status))
	    end
	  elseif op == "s" then
	    if status == "i" then
	      self:_cb("on_start", chan0)
	    else
	      assert(nil, ("UNHANDLED op %s, status %s"):format(op, status))
	    end
	  else
	    assert(nil, ("UNHANDLED op %s, status %s"):format(op, status))
	  end
	end
      else
	assert(nil, "UNHANDLED ch#"..chno)
      end
    elseif op == "answered" then
      -- All sent MSGs have been answered in full.
      -- (Safe to initiate close, tuning reset, etc.)
      --
      -- We need to remember this state so we know when we can close a channel.
    elseif op == "qempty" then
      -- All queued outgoing frames for this channel have been written.
      --
      -- Useful so local side can get involved in flow control?
    elseif op == "quiescent" then
      -- Channel is quiescent.
      --
      -- (Safe to confirm close, tuning reset, etc.)
      --
      -- We need to remember this state so we know when we can confirm close a channel.
    else
      assert(nil, "UNHANDLED "..op)
    end
  end
end

function mt:frame_read()
  return self.core:frame_read()
end

function mt:chan0_read()
  return self.core:chan0_read()
end

function mt:chan_start(arg)
  return self.core:chan_start(arg.profiles, arg.server_name, arg.channel_number)
end

function mt:status()
  return self.core:status()
end

-- FIXME how can I define userdata objects, so their methods can be defined
-- in lua!
function swirl.session(arg)
  local self = setmetatable({
    arg=arg,
    upper = {}, -- upper layer notifications, pairs of {op, chno}
    lower = {}, -- lower layer notifications, op=true when pending
  }, mt)
  local function notify_lower(op)
    print("["..tostring(self.core).."] cb lower "..op)
    self.lower[op] = true
  end
  local function notify_upper(chno, op)
    print("["..tostring(self.core).."] cb upper ch#"..chno.." "..op)
    table.insert(self.upper, {chno, op})
  end

  self.core = swirl.core(
    notify_lower,
    notify_upper,
    arg.il,
    nil, -- features
    nil, -- localize
    arg.profile or {}
    )

  return self
end


-- debug wrappers

function create(arg)
  local c = swirl.session(arg)

  print("create "..q(c))

  return c
end

function pull(c)
  local b = c:pull()

  if b then
    print("["..tostring(c).."] > \n"..b)
  end

  return b
end

function push(c, b)
  print("["..tostring(c).."] < ["..#b.."])")
  c:push(b)
end

function dolower(i, l)
  local function pullpush(from, to)
    local b = pull(from)
    if b then
      push(to, b)
    end
    return b
  end

  while pullpush(i, l) or pullpush(l,i) do
    print"... lower looped"
  end
end

function frame_read(c)
  local f = c:frame_read()
  print("["..tostring(c).."] > "..tostring(f))
  print(f:payload())
  return f
end

function chan0_read(c)
  local f = c:chan0_read()
  print("["..tostring(c).."] > "..tostring(f))
  for i,v in ipairs(f:profiles()) do
    print("    profile="..q(v))
  end
  return f
end

print"= session establishment..."

i = create{il="I"}
l = create{
  il="L",
  on_accepted = function() print"L ACCEPTED" end,
  on_start = function(ch0) 
    print("...on_start:", ch0:channelno(), q(ch0:profiles()), q(ch0:servername()))
    ch0:accept(ch0:profiles()[1])
    --ch0:reject(1, "error message here")
  end,
}

print("I="..tostring(i))
print("L="..tostring(l))

dolower(i,l)

print"= channel start..."

chno = i:chan_start{
  profiles={{uri="http://example.org/beep/echo"}},
  servername="beep.example.com",
  -- callbacks?
  }

print("= requested chno "..chno)

dolower(i,l)

print("= L is "..q(l))

dolower(i,l)

-- test gc order
print"no gc"

i = nil
l = nil

print"yes gc"

fi = nil

collectgarbage()

print"done"

