--[[
benchmark comparing single channel msg/rpy over beep with similar over tcp

tcp just writes the data raw, with no framing, then writes a single byte back

this isn't a comparison of beep against another protocol, but since we can't go
faster than tcp, it gives an idea how close we are getting to the theoretical
max

That seems to be about 30% of max, not sure if this is good, but it isn't
awful, either.

Profiling is required!


ensemble:~/p/chat/git-chat/swirl % lua bm-server.lua tcp 4321 localhost 5
done t=6 msgs=5745 msgs/sec=957.500000 bytes/sec=4787500.000000

ensemble:~/p/chat/git-chat/swirl % lua bm-server.lua beep 4321 localhost 5
done t=6 msgs=1785 msgs/sec=297.500000 bytes/sec=1487500.000000

ensemble:~/p/chat/git-chat/swirl % lua bm-server.lua beep 4321 localhost 5
done t=6 msgs=3693 msgs/sec=615.500000 bytes/sec=307750.000000

ensemble:~/p/chat/git-chat/swirl % lua bm-server.lua tcp 4321 localhost 5
done t=6 msgs=7306 msgs/sec=1217.666667 bytes/sec=608833.333333
]]

require"swirl"
require"socket"

require"table2str"

-- Quote a string into lua form (including the non-printable characters from
-- 0-31, and from 127-255).
function q(_)
  local fmt = string.format
  local _ = _ --fmt("%q", _)

  _ = string.gsub(_, "\n", "\\n")
  _ = string.gsub(_, "\r", "\\r")
  _ = string.gsub(_, "[%z\1-\31,\127-\255]", function (x)
    --print("x=", x)
    return fmt("\\%03d",string.byte(x))
  end)

  return _
end

local function q(t)
  return serialize(t)
end

kind = assert(arg[1], "kind is beep or tcp")
port = assert(arg[2], "port to listen/connect")
host = arg[3]
time = tonumber(arg[4]) or 5

--CHUNK = string.rep("x", 50 * 1024 * 1024)
CHUNK = string.rep("x", 50):rep(1024):rep(1024)

URI = "http://example.com/beep/null"

function tstart()
  t0 = os.time()
  msgs = 0
end

function tstop()
  msgs = msgs + 1
  t = os.time() - t0
  print("tstop", os.time(), t0, t, msgs)
  if t > time then
    io.write(string.format("\n\ndone t=%d msgs=%d msgs/sec=%f bytes/sec=%f\n",
      t, msgs, msgs/t, msgs * #CHUNK / t))
    os.exit(0)
  end
end


-- FIXME how do I know when I can tcp shutdown?
on = true

if kind == "beep" then
  function session(il, on_connected)
    return swirl.session{il=il,
      on_connected = function(...)
	print("on_greeting", ...)
	if il == "I" then
	  core:start{profiles = {{uri=URI}}}
	end
      end,

      on_start = function(ch0)
	print("... on_start:", ch0:channelno(), q(ch0:profiles()), q(ch0:servername()))
	for i, profile in pairs(ch0:profiles()) do
	  if profile.uri == URI then
	    ch0:accept(profile.uri)
	    return
	  end
	end
	ch0:reject(500, "unsupported") -- FIXME add these error codes to swirl.lua!
      end,

      on_close = function(ch0)
	local chno = ch0:channelno()
	local ecode, emsg, elang = ch0:error()
	print("... on_close:", chno, ecode, q(emsg), elang)
	ch0:accept()
      end,

      on_msg = function(frame)
	print("... on_msg:", frame:channelno(), frame:messageno(), frame:more(), #frame:payload())

	if not frame:more() then
	  frame:session():send_rpy(frame:channelno(), frame:messageno(), "")
	end

	frame:destroy()
      end,

      on_started = function(chno, uri, content)
	print("... on_started:", chno, uri, q(content))
	tstart()
	core:send_msg(chno, CHUNK)
      end,

      on_rpy = function(frame)
	print("... on_rpy:", frame:channelno(), frame:messageno(), frame:more(), #frame:payload())
	if not frame:more() then
	  tstop()
	  core:send_msg(frame:channelno(), CHUNK)
	end
      end,
    }
  end
else
  function session(il)
    local core = {}
    if il == "L" then
      local pull = nil
      function core:push(b)
	pull = "x"
      end
      function core:pull(b)
	local _ = pull
	pull = nil
	return _
      end
    else
      local pull = CHUNK
      local first = true
      function core:push(b)
	tstop()
	pull = CHUNK
      end
      function core:pull(b)
	if first then
	  tstart()
	  first = nil
	end
	local _ = pull
	pull = nil
	return _
      end
    end
    return core
  end
end

if not host then
  ssock = assert(socket.bind("*", port))
  sock = assert(ssock:accept())
  core = session("L")
else
  sock = assert(socket.connect(host, port))
  core = session("I")
end

sock:settimeout(0)

qrd = { sock }
qwr = { sock }

--print = function() end

while on do
  --print("select q", q(qrd), q(qwr))

  ard, awr, err = socket.select(qrd, qwr)

  --print("select a", q(ard), q(awr))

  if awr[sock] then
    local b = core:pull()
    if not b then
      qwr = {} -- core has nothing to send
    else
      print("send", #b)
      -- actually, rpy is zero size, so we never have to send SEQ to open more
      -- space
      if b:match("^SEQ") then
	print(b)
      end
      assert(sock:send(b))
    end
  end

  if ard[sock] then
    local b, emsg, partial = sock:receive(#CHUNK * 2)
    print("recv", q(b), emsg, q(partial))

    if emsg == "timeout" and #partial > 0 then
      assert(not b, emsg)
      b = partial
      emsg = nil
    end
    if emsg == "closed" then
      os.exit()
    end
    assert(not emsg, emsg)
    if b then
      core:push(b)
      qwr = { sock } -- core may have something to send
    end
  end
end

