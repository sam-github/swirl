io.stdout:setvbuf"line"

CHAT = "http://example.com/beep"

USER = ...
assert(USER, "usage: cx.lua <USER>")

require"vortex"

-- vortex:log_enable"yes"

print("register: "..CHAT.." for "..USER)

vortex:profiles_register{
  profile = CHAT,
  start = function (...)
    print("start - ", ...)
    return true
  end,
  close = function (...)
    print("close - ", ...)
    vortex:listener_unlock()
    return true
  end,
  frame = function (chan, conn, frame)
    print("frame #"..frame:msgno()..":"..frame:type().."<"..frame:payload()..">")
    chan:close(function(...)
      print("closed - ",...)
      local conn = ...
      print("conn:close() -> ", conn:close())
    end)
    print("chan:close()...")
  end,
}

vortex:connection_new{
  port=44000,
  on_connected = function (conn)
    print("connected - ", conn, " ok? ", conn:ok())
    conn:channel_new{
      profile=CHAT,
      encoding=none,
      content=USER,
      on_created = function(chan)
        print("created - ", chan)
        chan:send_msg"my captain, my captain!"
      end,
    }
  end,
}

print"listener wait.."

vortex:listener_wait()

print"listener woke"

for k, v in pairs(vortex._objects) do
  print(v)
end

vortex:exit()

print"exited"

-- vim:ft=lua:sw=2:sts=2:et:
