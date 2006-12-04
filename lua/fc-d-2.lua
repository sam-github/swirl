io.stdout:setvbuf"line"

CHAT = "http://example.com/beep"

require"vortex"

--vortex:log_enable"yes"

print("register: "..CHAT)

vortex:profiles_register{ 
  profile = CHAT,
  frame = function (chan, conn, frame)
    print("frame - ", chan, conn, frame)
    print("frame#"..frame:msgno()..":"..frame:type().."<"..frame:payload()..">")
    --chan:send_rpy(frame:msgno(), "ok")
  end,
}

vortex:listener_new{port=44000}

print"listener wait.."

vortex:listener_wait()

-- vim:ft=lua:sw=2:sts=2:et:
