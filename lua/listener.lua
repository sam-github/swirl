require"vortex"

users = {}

vortex:init()

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
    return true -- , "content reply"
  end,
  close = function (...)
    -- print varargs!
    print"close..."
    return true
  end,
  frame = function (channel, connection, frame)
    print("frame#"..frame:msgno().."<"..frame:payload()..">")
    channel:send_rpy(frame:msgno(), "ok")
    print"rpy <ok>"
  end,
}

vortex:listener_new("0.0.0.0", 44000)

--[[, {
  ready = function () print "ready..." end,
  accepted = function () print "accepted..." end,
}
]]

print"listener wait.."

vortex:listener_wait()

-- How do we exit?

print"listener wait done"

