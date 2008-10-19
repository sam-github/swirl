--[[

frame:send_rpy()/err/ans/null? better than frame:session():send_rpy..
]]

local getmetatable = getmetatable

local swirl = require"swirl.core"

module("swirl", package.seeall)

local core = getmetatable(swirl._core(function()end, function()end))

function core:_cb(cb, ...)
  --print("-> _cb "..cb, ...)
  -- TODO callbacks might all need to take session as the first argument,
  -- because they can't capture the session in their closure (they are passed
  -- before the session exists)
  local fncb = self._arg[cb]
  if fncb then
    ok, msg = pcall(fncb, ...)
    if not ok then
      (self._arg.on_error or print)(cb.." failed with "..msg)
    end
  end
end

function core:pull()
  self._lower.outready = false
  return self:_pull()
end

function core:push(buffer)
  self._lower.inready = nil
  self:_push(buffer)

  -- process upper layer...
  while #self._upper > 0 do
    local chno, op = unpack(table.remove(self._upper))
    --print("  upper ch#"..chno.." "..op)
    if op == "frame" or op == "message" then
      if chno == 0 then
	local chan0 = self:_chan0_read()
	if chan0 then
	  print(tostring(chan0))
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
		--self:destroy()
	      end
	    else
	      assert(nil, ("UNHANDLED op %s, ic %s"):format(op, ic))
	    end
	  else
	    assert(nil, ("UNHANDLED op %s, ic %s"):format(op, ic))
	  end
	end
      else
	local frame = self:_frame_read(chno)
	if frame then
	  self:_cb("on_"..frame:type(), frame)
	end
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

--[[
- chno = session:start(URI, ...)
- chno = session:start{profiles={URI, ...}, [servername=STR], [chno=NUM]}

Profiles is a list of URIs identifying profiles desired.

Should be an easy way of calling for the common case:
1 - just a single URI
2 - a single URI, with payload data
3 - fully parameterized, multiple profiles w/payload, servername, chno, etc.

BEEP calls this start and close. Too bad, I think it should be open and close, or
start and stop!
]]
function core:start(arg)
  return self:_chan_start(arg.profiles, arg.servername, arg.channelno)
end

--[[

chno defaults to 0

TEST what happens when you close channel 0, but other channels are open? what
should happen?

TEST what happens when you close a channel that isn't open, or close twice a
channel? Might need to track at this level and forbid.
]]
function core:close(chno, ecode)
  return self:_chan_close(chno or 0, ecode)
end

function core:send_msg(chno, msg)
  -- We are responsible for allocating message numbers, lets make them increase
  -- sequentially within a channel.
  local msgno = self._msgno[chno] or 1
  self:_frame_send(chno, "MSG", msg, msgno)
  self._msgno[chno] = msgno + 1
  return msgno
end

function core:send_rpy(chno, msgno, rpy)
  return self:_frame_send(chno, "RPY", rpy, msgno)
end

function core:send_err(chno, msgno, err)
  return self:_frame_send(chno, "ERR", err, msgno)
end


local function notify_lower(self, op)
  --print("["..tostring(self.core).."] cb lower "..op)
  if self then
    self._lower[op] = true
  end
end

local function notify_upper(self, chno, op)
  --print("["..tostring(self.core).."] cb upper ch#"..chno.." "..op)
  if self then
    table.insert(self._upper, {chno, op})
  end
end

function session(arg)
  -- The notify callbacks need to have a reference to their core, usually self,
  -- but we haven't created it yet! Declare the variable, so they can see it
  -- once it's initialized.
  local self = nil

  self = swirl._core(
    function(op) notify_lower(self, op) end,
    function(chno, op) notify_upper(self, chno, op) end,
    arg.il,
    nil, -- features
    nil, -- localize
    arg.profile or {}
    )

  self._arg=arg
  self._upper = {} -- upper layer notifications, pairs of {op, chno}
  self._lower = {} -- lower layer notifications, op=true when pending
  self._msgno = {} -- map chno to the highest msgno that has been used
  -- TODO - if channel is closed, chno can be reused, so we need to clear these on close!

  return self
end
