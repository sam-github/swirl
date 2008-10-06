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
    --print("  upper ch#"..chno.." "..op)
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
	    -- Greeting
	    if status == "c" then
	      self:_cb("on_accepted")
	    elseif status == "e" then
	      self:_cb("on_rejected") -- TODO pass the error message
	    else
	      assert(nil, ("UNHANDLED op %s, status %s"):format(op, status))
	    end
	  elseif op == "s" then
	    -- Start
	    if status == "i" then
	      self:_cb("on_start", chan0)
	    elseif status == "c" then
	      local p = chan0:profiles()[1]
	      local ecode, emsg, elang = chan0:error()
	      local chno = chan0:channelno()
	      if p then
		-- blu_chan0_in() docs indicate there may be an error, too, I think that would
		-- just happen if the RPY payload contained xml that looked like <error>.
		self:_cb("on_started", chno, p.uri, p.content)
	      else
		assert(ecode, ("UNHANDLED op %s, status %s"):format(op, status))
		self:_cb("on_startfailed", chno, ecode, emsg, elang)
	      end
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

    on_close = function(chno, ...)
      print("... on_close:", chno, ...)
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

function dolower(i, l)
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

dolower(i,l)


print"=== test channel start ok"

i = create{il="I"}
l = create{il="L"}

dolower(i,l)

chno = i:chan_start{
  profiles={{uri="http://example.org/beep/echo", content="CONTENT"}},
  servername="SERVERNAME",
  }

dolower(i,l)


print"=== test channel start error"

i = create{il="I"}
l = create{il="L",
  on_start = function(ch0)
    ch0:reject(500)
  end,
}

dolower(i,l)

chno = i:chan_start{
  profiles={{uri="http://example.org/beep/echo"}},
  }

dolower(i,l)




print"=== garbage collect"

i = nil
l = nil

collectgarbage()

print"=== done"

