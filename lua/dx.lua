io.stdout:setvbuf"line"

require"vortex"

print("register: ".."http://beep.ensembleindependant.org/profiles/chat")

vortex:profiles_register{ 
  profile = "http://beep.ensembleindependant.org/profiles/chat",
  start = function (profile, channum, conn, server, name, encoding)
    print(
      "start"..
      " #"..channum..
      " profile="..profile..
      " server="..tostring(server)..
      " content="..name..
      " encoding="..encoding
    )
    --vortex:listener_unlock()
    return true
  end,
  close = function (...)
    --vortex:listener_unlock()
    return true
  end,
  frame = function (chan, conn, frame)
    print("frame#"..frame:msgno()..":"..frame:type().."<"..frame:payload()..">")
    chan:send_rpy(frame:msgno(), "ok")
    if frame:payload() == "x" then
      print"..."
      print("conn:close() -> "..tostring(conn:close()))
      vortex:listener_unlock()
      print"vortex:listener_unlock() done"
    end
  end,
}

vortex:listener_new{port=44000}

print"listener wait.."

vortex:listener_wait()

print"listener woke"

vortex:exit()

print"exited"

-- vim:ft=lua:
