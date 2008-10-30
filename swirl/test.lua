require"swirl"

require"table2str"

local function q(t)
  return serialize(t)
end

function create(arg)
  -- map msgno to incomplete messages
  local msg_accum = {}
  local template = {
    -- session management
    on_connected = function(profiles, features, localize)
      print("...on_connected", q(profiles), q(features), q(localize))
    end,

    on_connect_err = function(...)
      print("... on_connect_err", ...)
    end,

    -- channel management
    on_start = function(ch0)
      print("... on_start:", ch0:channelno(), q(ch0:profiles()), q(ch0:servername()))

      -- accept the first of the requested profiles
      local p = ch0:profiles()[1]
      ch0:accept(p.uri, p.content)
    end,

    on_started = function(chno, uri, content)
      print("... on_started:", chno, uri, q(content))
    end,

    on_start_err = function(chno, ecode, emsg, elang)
      print("... on_start_err:", chno, ecode, q(emsg), elang)
    end,

    on_close = function(ch0)
      local chno = ch0:channelno()
      local ecode, emsg, elang = ch0:error()
      print("... on_close:", chno, ecode, q(emsg), elang)
      ch0:accept()
    end,

    on_closed = function(chno, ...)
      print("... on_closed:", chno, ...)
    end,

    -- message transfer
    on_msg = function(frame)
      print("... on_msg:", frame:channelno(), frame:messageno(), frame:more(), q(frame:payload()))
      -- frame accumulation
      local chno, msgno, more, msg = 
        frame:channelno(), frame:messageno(), frame:more(), frame:payload()

      if not msg_accum[chno] then msg_accum[chno] = {} end
      if not msg_accum[chno][msgno] then msg_accum[chno][msgno] = "" end

      msg_accum[chno][msgno] =  msg_accum[chno][msgno]..frame:payload()

      if not frame:more() then
	frame:session():send_rpy(frame:channelno(), frame:messageno(), msg_accum[chno][msgno])
	msg_accum[chno][msgno] = nil
      end

      frame:destroy()
    end,

    on_rpy = function(frame)
       print("... on_rpy:", frame:channelno(), frame:messageno(), frame:more(), q(frame:payload()))
       frame:destroy()
     end,

    on_err = function(frame)
       print("... on_err:", frame:channelno(), frame:messageno(), frame:more(), q(frame:payload()))
       frame:destroy()
     end,
  }

  for k,v in pairs(arg) do template[k] = v end

  return swirl.session(template)
end

function pump(i, l, quiet)
  local function pullpush(from, to)
    local b = from:pull()
    if b then
      if not quiet then
	print("-- "..from._arg.il.." to "..to._arg.il.."\n"..b.."--")
      end
      assert(to:push(b))

      from:pulled(#b)
    end
    return b
  end

  while pullpush(i, l) or pullpush(l,i) do
  end
end

print"\n\n=== test independence of core env"

do
  local function p(c)
    local env = debug.getfenv(c)
    local meta = getmetatable(c)
    print("p", c, meta, env)
    for k,v in pairs(env) do print("env", k, v) end
    --for k,v in pairs(meta) do print("meta", k, v) end
  end

  local function f() end

  i = swirl._core(f, f)
  --p(i)
  l = swirl._core(f, f)
  --p(i)
  --p(l)

  i.n = "i"
  assert(i.n == "i")

  l.n = "l"
  assert(i.n == "i")
  assert(l.n == "l")
end

print"\n\n=== test connect/ok"

i = create{il="I"}
l = create{il="L"}

pump(i,l)

print"\n\n=== test connect/ok w/zero profiles"

ok = 0

i = create{il="I", profile={},
  on_connected=function(profiles, features, localize)
    print("on_connected", q(profiles), q(features), q(localize))
    ok = ok + #profiles
  end,
}

l = create{il="L",
  on_connected=function(profiles, features, localize)
    print("on_connected", q(profiles), q(features), q(localize))
    ok = ok + #profiles
  end,
}

pump(i,l)

assert(ok == 0)


print"\n\n=== test connect/ok w/one+one profiles"

ok = 0

i = create{il="I", profile={"http://example.com/I"},
  on_connected=function(profiles, features, localize)
    print("on_connected", q(profiles), q(features), q(localize))
    ok = ok + #profiles
  end,
}

l = create{il="L", profile={"http://example.com/L"},
  on_connected=function(profiles, features, localize)
    print("on_connected", q(profiles), q(features), q(localize))
    ok = ok + #profiles
  end,
}

pump(i,l)

assert(ok == 2)


print"\n\n=== test connect/ok w/zero+two profiles"

ok = 0

i = create{il="I",
  on_connected=function(profiles, features, localize)
    print("on_connected", q(profiles), q(features), q(localize))
    ok = ok + #profiles
  end,
}

l = create{il="L", profile={"http://example.com/L1", "http://example.com/L2"},
  on_connected=function(profiles, features, localize)
    print("on_connected", q(profiles), q(features), q(localize))
    ok = ok + #profiles
  end,
}

pump(i,l)

assert(ok == 2)


print"\n\n=== test connect/err"

ok = nil

i = create{il="I",
  on_connect_err=function(ecode, emsg, elang)
    print("on_connect_err", q(ecode), q(emsg), q(elang))
    ok = true
  end,
}

l = create{il="L", error={501}}

pump(i,l)

assert(ok)


print"\n\n=== test session close"

i = create{il="I"}
l = create{il="L"}

pump(i,l)

i:close(0)

pump(i,l)

-- check that the session is now not working
chno = i:start{
  profiles={{uri="http://example.org/beep/echo"}},
  }

pump(i,l)


print"\n\n=== test session close/err"

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
chno = i:start{
  profiles={{uri="http://example.org/beep/echo"}},
  }

pump(i,l)


print"\n\n=== test start/ok"

ok = nil

i = create{il="I",
  on_started = function(...)
     print("on_started", ...)
   ok = true
  end,
}
l = create{il="L"}

pump(i,l)

chno = i:start{
  profiles={{uri="http://example.org/beep/echo", content="CONTENT"}},
  servername="SERVERNAME",
  }

pump(i,l)

assert(ok)


print"\n\n=== test start/err"

ok = nil

i = create{il="I",
  on_start_err = function(chno, ecode, emsg, elang)
    print("on_start_err", chno, ecode, emsg, elang)
    ok = (ecode == 500 and chno == 1)
  end,
}

l = create{il="L",
  on_start = function(ch0)
    print("on_start", ch0)
    ch0:reject(500)
  end,
}

pump(i,l)

chno = i:start{
  profiles={{uri="http://example.org/beep/echo"}},
  }

pump(i,l)

assert(ok, "channel start error")


print"\n\n=== test msg send/rpy"

ok = nil

i = create{il="I",
  on_rpy=function(frame)
    print("on_rpy", frame)
    ok = frame
  end,
}
l = create{il="L"}

pump(i,l)

chno = i:start{
  profiles={{uri="http://example.org/beep/echo", content="CONTENT"}},
  servername="SERVERNAME",
  }

pump(i,l)

msgno = i:send_msg(chno, "hello, world")

pump(i,l)

assert(ok)


print"\n\n=== test msg send/err"

ok = false

i = create{il="I",
  on_err = function(frame)
    print("on_err", frame)
    ok = frame
  end
}

l = create{il="L",
  on_msg = function(frame)
    frame:session():send_err(frame:channelno(), frame:messageno(), "ERROR MSG")
    frame:destroy()
  end
}

pump(i,l)

chno = i:start{
  profiles={{uri="http://example.org/beep/echo", content="CONTENT"}},
  servername="SERVERNAME",
  }

pump(i,l)

msgno = i:send_msg(chno, "hello, world")

pump(i,l)

assert(ok)


print"\n\n=== tests msg send/rpy +4K"

msg = nil

sz = 4097

i = create{il="I",
  on_rpy=function(frame)
    print("on_rpy",frame:channelno(), frame:messageno(), frame:more(), #frame:payload())
    msg = msg..frame:payload()
    saveframe = frame
  end,
}

l = create{il="L"}

pump(i,l)

chno = i:start{profiles={{uri="http://example.org/beep/echo"}}}

pump(i,l)

print("=sz "..sz)

local sent = ("x"):rep(sz)

msgno = i:send_msg(chno, sent)

msg = ""

pump(i,l)

print("+destroy frame")

saveframe:destroy()

pump(i,l)

if sent ~= msg  then
  print("send", #sent, "recv", #msg)
end

assert(msg == sent)



print"\n\n=== garbage collect"

i = nil
l = nil

collectgarbage()


print"\n\n=== done"


