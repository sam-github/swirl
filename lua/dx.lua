io.stdout:setvbuf"line"

CHAT = "http://example.com/beep"

require"vortex"

--vortex:log_enable"yes"

print("register: "..CHAT)

vortex:profiles_register{ 
  profile = CHAT,
  start = function (...)
    print("start - ",...)
    return true
  end,
  close = function (...)
    print("close - ", ...)
    --[[
    chan, conn = ...
    print("conn:close() -> ")
    print(tostring(conn:close()))
    vortex:listener_unlock()
    ]]
    return true
  end,
  frame = function (chan, conn, frame)
    print("frame - ", chan, conn, frame)
    print("frame#"..frame:msgno()..":"..frame:type().."<"..frame:payload()..">")
    chan:send_rpy(frame:msgno(), "ok")
    --[[
    if frame:payload() == "x" then
      print"..."
      print("conn:close() -> "..tostring(conn:close()))
      vortex:listener_unlock()
      print"vortex:listener_unlock() done"
    end
    ]]
  end,
}

vortex:listener_new{port=44000}

print"listener wait.."

vortex:listener_wait()

print"listener woke"

vortex:exit()

print"exited"

-- vim:ft=lua:sw=2:sts=2:et:
