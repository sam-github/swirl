require"swirl"

require"table2str"

local function q(t)
  return serialize(t)
end

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
      print("... on_started:", chno, uri, q(content))
    end,

    on_startfailed = function(chno, ecode, emsg, elang)
      print("... on_startfailed:", chno, ecode, q(emsg), elang)
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
      frame:session():send_rpy(frame:channelno(), frame:messageno(), frame:payload())
      frame:destroy()
    end,

    on_rpy = function(frame)
       print("... on_rpy:", frame:channelno(), frame:messageno(), frame:more(), q(frame:payload()))

       frame:destroy()
     end
  }

  for k,v in pairs(arg) do template[k] = v end

  return swirl.session(template)
end

function pump(i, l)
  local function pullpush(from, to)
    local b = from:pull()
    if b then
      print("-- "..from._arg.il.." to "..to._arg.il.."\n"..b.."--")
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

chno = i:start{
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

chno = i:start{
  profiles={{uri="http://example.org/beep/echo"}},
  }

pump(i,l)


print"=== test session close"

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


print"=== test session close with rejection"

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


print"=== test msg send"

i = create{il="I"}
l = create{il="L"}

pump(i,l)

chno = i:start{
  profiles={{uri="http://example.org/beep/echo", content="CONTENT"}},
  servername="SERVERNAME",
  }

pump(i,l)

msgno = i:send_msg(chno, "hello, world")

pump(i,l)

print"=== garbage collect"

i = nil
l = nil

collectgarbage()

print"=== done"

