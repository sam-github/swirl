--[[

frame:send_rpy()/err/ans/null? better than frame:session():send_rpy..
]]

local getmetatable = getmetatable

local swirl = require"swirl.core"

module("swirl", package.seeall)

local core = getmetatable(swirl._core(function()end, function()end))

function core:_cb(cb, ...)
  --print("-> _cb "..cb, ...)
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
	      local profiles = {}
	      for i,profile in ipairs(chan0:profiles()) do
		table.insert(profiles, profile.uri)
	      end
	      local ecode = chan0:error()
	      if not ecode then
		self:_cb("on_connected", profiles, chan0:features(), chan0:localize())
		chan0:destroy()
	      else
		self:_cb("on_connect_err", chan0:error())
		chan0:destroy()
	      end
	    else
	      -- BEEP doesn't have a greeting indication, this is a bug!
	      assert(nil, ("BUG op %s, ic %s"):format(op, ic))
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
		self:_cb("on_start_err", chno, ecode, emsg, elang)
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
    elseif op == "qempty" then
      -- All queued outgoing frames for this channel have been written.
      --
      -- Useful so local side can get involved in flow control?
    elseif op == "answered" then
      -- All sent MSGs have been answered in full.
      -- (Safe to initiate close, tuning reset, etc.)
      --
      -- We need to remember this state so we know when we can close a channel.
    elseif op == "quiescent" then
      -- Channel is quiescent.
      --
      -- (Safe to confirm close, tuning reset, etc.)
      --
      -- We need to remember this state so we know when we can confirm close a channel.
    elseif op == "windowfull" then
      -- Receive window has filled up.
      --
      -- What does this mean?
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

--[[
- msgno = core:send_msg(chno, msg)

chno is the channel on which the message is being sent
msg is the data to be sent

TODO - it would be possible to allow partial send_msg, if msgno and more are provided
]]
function core:send_msg(chno, msg)
  chno = tonumber(chno)
  assert(chno and chno > 0)
  -- We are responsible for allocating message numbers, lets make them increase
  -- sequentially within a channel.
  local msgno = self._msgno[chno] or 1
  self:_frame_send(chno, "MSG", msg, msgno, nil, more)
  self._msgno[chno] = msgno + 1
  return msgno
end

--[[
- msgno = core:send_rpy(chno, msgno, rpy, more)

chno is the channel on which the message is being sent
msgno is the number of the msg being replied to
rpy is the data to be sent
more is whether there is more data to be sent in this reply, it defaults
  to false
]]
function core:send_rpy(chno, msgno, rpy, more)
  return self:_frame_send(chno, "RPY", rpy, msgno, nil, more)
end

function core:send_err(chno, msgno, err)
  return self:_frame_send(chno, "ERR", err, msgno)
end


local function notify_lower(lower, op, ...)
  --print("["..tostring(self.core).."] cb lower "..op)
  if op == "status" then
    local status, text, chno = ...
    lower.status = {status=status, text=text, chno=chno}
    print("STATUS", ...)
  else
    lower[op] = true
  end
end

local function notify_upper(upper, chno, op)
  --print("["..tostring(self.core).."] cb upper ch#"..chno.." "..op)
  table.insert(upper, {chno, op})
end

--[[
- core = swirl.session{...}

TODO fail if unexpected arguments are received, to catch typos

Arguments:
  il=[I|L] whether this session is an initiator or listener, default is initiator

  profile = [{uri, ...}] a list of URIs for the server profiles supported locally, optional

    TODO pluralize to "profiles"?

  error = {ecode, emsg, elang}: a listener may refuse to accept a connection, providing
    an indication why
      ecode: error code
      emsg: textual message, optional
      elang: language of textual message, optional

  on_connected=function(profiles, features, localize) called on successful greeting from peer
    profiles: an array of URIs identifying the server profiles supported by the peer
    features: a string of tokens describing optional features of the channel
      management profile, or nil (rarely supported)
    localize: a string of tokens describing languages preferred in textual elements of close
      and error messages, or nil (rarely supported)
    TODO - greeting info should be saved for later querying?
    TODO - is it a BEEP protocol error to request a connection for an unadvertised profile?

  on_connect_err=function(ecode, emsg, elang): listener refused to accept the connection

  on_start=function(ch0) called with a request to start a channel, respond by
    calling ch0:accept() or ch0:reject()

  on_started=function(chno, uri, content?) called to confirm a positive response to
    a channel start request, uri is the selected profile, and content is optional

  on_start_err=function(chno, ecode, emsg, elang) called to confirm a negative response
    to a channel start request

TODO - above could be unified with signatures like:
             on_start(chno, uri|nil, content|ecode, nil|emsg, nil|elang)
             on_start(chno, uri|nil, { content=, ecode=, emsg=, elang=})
             on_start(chno, profile|err)
	       profile={uri=, content=}
	       err={ecode=,emsg=,elang=}

  on_close = function(ch0) 

  on_closed = function()

  on_close_err = 

  on_msg = function(frame)

]]
function session(arg)
  -- The notify callbacks are called during core creation, so make sure that
  -- they have a place to put their notifications before the core is returned
  -- to us.
  local lower = {}
  local upper = {}

  local self = swirl._core(
    function(...) notify_lower(lower, ...) end,
    function(chno, op) notify_upper(upper, chno, op) end,
    arg.il,
    nil, -- features
    nil, -- localize
    arg.profile or {},
    arg.error
    )

  self._arg=arg
  self._lower = lower -- lower layer notifications, op=true when pending
  self._upper = upper -- upper layer notifications, pairs of {op, chno}
  self._msgno = {}    -- map chno to the highest msgno that has been used
  -- TODO - if channel is closed, chno can be reused, so we need to clear these on close!

  return self
end

