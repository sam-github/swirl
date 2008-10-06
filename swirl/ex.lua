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
  --print("-> _cb "..cb, ...)
  -- TODO callbacks might all need to take session as the first argument,
  -- because they can't capture the session in their closure (they are passed
  -- before the session exists)
  -- TODO should pcall these. if error raised, call on_error (without pcall)
  -- (so user can choose whether to log error somewhere, or to ignore, exit,
  -- whatever)
  cb = self.arg[cb]
  if cb then
    ok, msg = pcall(cb, ...)
    if not ok then
      (self.arg.on_error or print)(cb.." failed with "..msg)
    end
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
    --print("  upper ch#"..chno.." "..op)
    if op == "frame" or op == "message" then
      if chno == 0 then
	local chan0 = self:chan0_read()
	if not chan0 then
	  -- message could already have been consumed
	else
	  print(q(chan0))
	  local ic, op = chan0:op()
	  print("op "..op.." ic "..ic)

	  if op == "g" then
	    -- Greeting
	    if ic == "c" then
	      self:_cb("on_accepted")
	    elseif ic == "e" then
	      self:_cb("on_rejected") -- TODO pass the error message
	    else
	      assert(nil, ("UNHANDLED op %s, ic %s"):format(op, ic))
	    end
	  elseif op == "s" then
	    -- Start
	    if ic == "i" then
	      self:_cb("on_start", chan0)
	    elseif ic == "c" then
	      local p = chan0:profiles()[1]
	      local ecode, emsg, elang = chan0:error()
	      local chno = chan0:channelno()
	      if p then
		-- blu_chan0_in() docs indicate there may be an error, too, I think that would
		-- just happen if the RPY content contained xml that looked like <error>, but
		-- I'm going to leave content decoding to the caller, and not try and return it.
		self:_cb("on_started", chno, p.uri, p.content)
	      else
		assert(ecode)
		self:_cb("on_startfailed", chno, ecode, emsg, elang)
	      end
	      chan0:destroy() -- free up beepcore memory, don't wait for gc
	    else
	      assert(nil, ("UNHANDLED op %s, ic %s"):format(op, ic))
	    end
	  elseif op == "c" then
	    if ic == "i" then
	      self:_cb("on_close", chan0)
	    elseif ic == "c" then
	      local chno = chan0:channelno()
	      self:_cb("on_closed", chno)
	      chan0:destroy() -- free up beepcore memory, don't wait for gc
	      if chno == 0 then
		--self.core:destroy()
	      end
	    else
	      assert(nil, ("UNHANDLED op %s, ic %s"):format(op, ic))
	    end
	  else
	    assert(nil, ("UNHANDLED op %s, ic %s"):format(op, ic))
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

-- TODO make private
function mt:frame_read()
  return self.core:frame_read()
end

-- TODO make private
function mt:chan0_read()
  return self.core:chan0_read()
end

function mt:chan_start(arg)
  return self.core:chan_start(arg.profiles, arg.servername, arg.channelno)
end

function mt:status()
  return self.core:status()
end

function mt:close(ecode)
  return self.core:chan_close(0,ecode)
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
    --print("["..tostring(self.core).."] cb lower "..op)
    self.lower[op] = true
  end
  local function notify_upper(chno, op)
    --print("["..tostring(self.core).."] cb upper ch#"..chno.." "..op)
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
  local template = {
    -- session management
    on_accepted = function(...)
      print("... on_accepted", ...)
    end,

    -- channel management
    on_start = function(ch0)
      -- TODO maybe the ch0 should be held internally, and :accept() should be called
      -- with a chno on the session. This would be more consistent, the other cbs get
      -- info ffrom ch0, not the UD itself
      print("... on_start:", ch0:channelno(), q(ch0:profiles()), q(ch0:servername()))

      -- accept the first of the requested profiles
      -- TODO accept is confusing with on_accepted above...
      --    on_confirmed and on_denied?
      local p = ch0:profiles()[1]
      ch0:accept(p.uri, p.content)
    end,

    on_started = function(chno, uri, content)
      print("... on_started:", chno, uri, content)
    end,

    on_startfailed = function(chno, ecode, emsg, elang)
      print("... on_startfailed:", chno, ecode, emsg, elang)
    end,

    on_close = function(ch0)
      local chno = ch0:channelno()
      local ecode, emsg, elang = ch0:error()
      print("... on_close:", chno, ecode, emsg, elang)
      ch0:accept()
    end,

    on_closed = function(chno, ...)
      print("... on_closed:", chno, ...)
    end,
  }

  for k,v in pairs(arg) do template[k] = v end

  local c = swirl.session(template)

  --print("create "..q(c))

  return c
end

function pump(i, l)
  local function pullpush(from, to)
    local b = from:pull()
    if b then
      print("-- "..from.arg.il.." to "..to.arg.il.."\n"..b.."--")
      to:push(b)
    end
    return b
  end

  while pullpush(i, l) or pullpush(l,i) do
  end
end

print"=== test session establishment"

i = create{il="I"}
l = create{il="L"}

pump(i,l)


print"=== test channel start ok"

i = create{il="I"}
l = create{il="L"}

pump(i,l)

chno = i:chan_start{
  profiles={{uri="http://example.org/beep/echo", content="CONTENT"}},
  servername="SERVERNAME",
  }

pump(i,l)


print"=== test channel start error"

i = create{il="I"}
l = create{il="L",
  on_start = function(ch0)
    ch0:reject(500)
  end,
}

pump(i,l)

chno = i:chan_start{
  profiles={{uri="http://example.org/beep/echo"}},
  }

pump(i,l)


print"=== test session close"

local first_close = true
i = create{il="I"}
l = create{il="L"}

pump(i,l)

i:close()

pump(i,l)

-- check that the session is now not working
chno = i:chan_start{
  profiles={{uri="http://example.org/beep/echo"}},
  }

pump(i,l)

print"=== test session close with rejection"

local first_close = true
i = create{il="I",
  on_close = function(ch0)
    local chno = ch0:channelno()
    local ecode, emsg, elang = ch0:error()
    print("... I on_close:", chno, ecode, emsg, elang)
    ch0:reject(501, "not ready to do this just yet")
  end,
}

l = create{il="L",
  on_close = function(ch0)
    local chno = ch0:channelno()
    local ecode, emsg, elang = ch0:error()
    print("... L on_close:", chno, ecode, emsg, elang)
    ch0:accept()
  end,
}

pump(i,l)

l:close()

pump(i,l)

i:close()

pump(i,l)

-- check that the session is now not working
chno = i:chan_start{
  profiles={{uri="http://example.org/beep/echo"}},
  }

pump(i,l)

print"=== garbage collect"

i = nil
l = nil

collectgarbage()

print"=== done"

