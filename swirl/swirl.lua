--[[-
Swirl - implementation of BEEP for Lua
]]
--[[
A binding of beepcore-c into Lua 5.1.

Copyright (c) 2008 Sam Roberts

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
]]


local getmetatable = getmetatable

local swirl = require"swirl.core"

module("swirl", package.seeall)

local function c(s)
  if not s then return "[]" end
  return "["..tostring(s).."+"..tostring(s._arg.il).."]"
end

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
  -- print(c(self), "pull")
  return self:_pull()
end

function core:pulled(sz)
  -- print(c(self), "pulled", sz)
  self._lower.outready = false
  return self:_pulled(sz)
end

function core:push(buffer)
  self._lower.inready = nil

  -- print(c(self), "push sz=", #buffer, "upper sz=", #self._upper)

  local ok, emsg = self:_push(buffer)

  if not ok then
    return nil, emsg
  end

  -- process upper layer...
  while #self._upper > 0 do
    local chno, op = unpack(table.remove(self._upper, 1))
    -- print(c(self), "do upper ch#", chno, op)
    if op == "frame" or op == "message" then
      if chno == 0 then
	local chan0 = self:_chan0_read()
	if chan0 then
	  --print(tostring(chan0))
	  local ic, op = chan0:op()
	  --print("op "..op.." ic "..ic)

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
		-- FIXME - first argument has to be self!
		self:_cb("on_started", chno, p.uri, p.content)
	      else
		assert(ecode)
		-- FIXME - first argument has to be self!
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

  return true
end

--[[
- chno = session:start(URI[, content=STR])

Start a channel with profile URI, and optional content.

- chno = session:start{profiles={uri=URI, content=STR}, [servername=STR], [chno=NUM]}

Start a channel with one of a set of profiles, an optional servername, and an
optional channel number.

TODO just make the arguments positional...

BEEP calls this start and close. Too bad, I think it should be open and close, or
start and stop!
]]
function core:start(...)
  local arg, content = ...
  if type(arg) == "string" then
    arg = {
      profiles = {
	{uri=arg, content=content}
      }
    }
  else
    assert(not content, "invalid extra argument")
  end

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
  chno = assert(tonumber(chno), "chno is not a number")
  assert(chno > 0, "chno is not greater than zero")
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


local opcb = {inready = "on_pushable", outready="on_pullable"}

local function notify_lower(lower, op, ...)
  if op == "status" then
    local status, text, chno = ...
    -- FIXME deal with errors!
    print(c(lower.self), "notify lower", op, status, text, chno)
    lower.status = {status=status, text=text, chno=chno}
  else
    -- print(c(lower.self), "notify lower", op)
    lower[op] = true
  end
  local cb = opcb[op]
  if cb and lower.self then
    lower.self:_cb(cb, lower.self)
  end
end

local function notify_upper(upper, chno, op)
  -- print(c(upper.self), "notify upper ch#", chno, op)
  table.insert(upper, {chno, op})
end

--[[
- core = swirl.session{...}

Arguments:
  il=[I|L] whether this session is an initiator or listener, default is initiator

  profile = [{uri, ...}] a list of URIs for the server profiles supported locally, optional

    TODO pluralize to "profiles"?

  error = {ecode, emsg, elang}: a listener may refuse to accept a connection, providing
    an indication why
      ecode: error code
      emsg: textual message, optional
      elang: language of textual message, optional

  on_connected=function(profiles, features, localize): called on successful greeting from peer
    profiles: an array of URIs identifying the server profiles supported by the peer
    features: a string of tokens describing optional features of the channel
      management profile, or nil (rarely supported)
    localize: a string of tokens describing languages preferred in textual elements of close
      and error messages, or nil (rarely supported)
    TODO - greeting info should be saved for later querying?
    TODO - is it a BEEP protocol error to request a connection for an unadvertised profile?

  on_connect_err=function(ecode, emsg, elang): listener refused to accept the connection

  on_start=function(ch0): called with a request to start a channel, respond by
    calling ch0:accept() or ch0:reject()

  on_started=function(chno, uri, content?): called to confirm a positive response to
    a channel start request, uri is the selected profile, and content is optional

  on_start_err=function(chno, ecode, emsg, elang): called to confirm a negative response
    to a channel start request

  on_close = function(ch0):

  on_closed = function():

  on_close_err = :

  on_msg = function(frame):


  on_pullable = function(sess): data is available via :pull(). Note carefully that no
    swirl calls may be made in this function. It is useful to adjust the select/poll
    set, for example, or to unblock a thread reading from the session, and writing to
    the peer. Sessions are always pullable after creation.

  on_pushable = function(sess): space is available for data to be :push()ed. The same
    notes as above apply to this callback. Sessions are always pushable after creation.

]]
function session(arg)
  -- The notify callbacks are called during core creation, so make sure that
  -- they have a place to put their notifications before the core is returned
  -- to us.
  local lower = { }
  local upper = { }

  assert(not arg.profiles)

  local self = swirl._core(
    function(...) notify_lower(lower, ...) end,
    function(chno, op) notify_upper(upper, chno, op) end,
    arg.il,
    nil, -- features
    nil, -- localize
    arg.profile or {},
    arg.error
    )

  -- so notifications can reach the session if they need to
  lower.self = self
  upper.self = self

  self._arg=arg
  self._lower = lower -- lower layer notifications, op=true when pending
  self._upper = upper -- upper layer notifications, pairs of {op, chno}
  self._msgno = {}    -- map chno to the highest msgno that has been used

  -- FIXME - if channel is closed, chno can be reused, so we need to clear these on close!

  return self
end

--[[
Reply codes from section 8 of RFC3080.

Some short hands are provided, but I'm not sure what short readable names I can
provide for the others. Suggestions?
]]
ecodes = {
  [200] = "success",
  ok = 200,

  [421] = "service not available",
  unavailable = 421,
  [450] = "requested action not taken", -- (e.g., lock already in use)
  [451] = "requested action aborted", -- (e.g., local error in processing)
  [454] = "temporary authentication failure",

  [500] = "general syntax error", -- (e.g., poorly-formed XML)
  [501] = "syntax error in parameters", -- (e.g., non-valid XML)
  invalidparam = 501,
  [504] = "parameter not implemented",
  [530] = "authentication required",
  [534] = "authentication mechanism insufficient", -- (e.g., too weak, sequence exhausted, etc.)
  [535] = "authentication failure",
  [537] = "action not authorized for user",
  [538] = "authentication mechanism requires encryption",
  [550] = "requested action not taken", -- (e.g., no requested profiles are acceptable)
  [553] = "parameter invalid",
  [554] = "transaction failed", -- (e.g., policy violation)
}
--[[ TODO use short names based on these?
#define BEEP_REPLY_SUCESS          200
#define BEEP_REPLY_NOT_TAKEN       450
#define BEEP_REPLY_ABORTED         451
#define BEEP_REPLY_AUTH_FAIL_TEMP  454
#define BEEP_REPLY_ERR_SYNTAX      500
#define BEEP_REPLY_ERR_PARAM       501
#define BEEP_REPLY_PARAM_NOT_IMPL  504
#define BEEP_REPLY_AUTH_REQD       530
#define BEEP_REPLY_AUTH_TOOWEAK    535
#define BEEP_REPLY_AUTH_FAILED     537
#define BEEP_REPLY_NOT_AUTORIZED   538
#define BEEP_REPLY_NO_ACTION       550
#define BEEP_REPLY_INVALID_PARAM   553
#define BEEP_REPLY_FAIL_TRANS      554
]]

--[[
When receiving TCP fragments from a socket, it is recommended to try to read at
least BUFSZ data at a time.
]]
-- Largest read should be default window size, plus largest BEEP line * 2.
BUFSZ = 4096 + 2 * #"ANS 2147483647 2147483647 . 4294967295 2147483647 2147483647\r\n"

