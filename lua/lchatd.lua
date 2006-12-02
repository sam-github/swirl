--[[
A simple IRC-like chat-server.
]]

io.stdout:setvbuf"line"

require"vortex"

chat = {
  users = {},
  conns = {},
}

function chat:start(profile, channo, conn, server, user, encoding)
  self.users[user] = { user = user, conn = conn, channo = channo, queue = {} }
  self.conns[conn] = self.users[user]
  return true
end

function chat:close(chan, conn)
  self.users[self.conns[conn].user] = nil
  self.conns[conn] = nil
  return true
end

function chat:frame(chan, conn, frame)
  local f =  self[frame:type()] or print
  return f(self, chan, conn, frame)
end

function chat:msg(chan, conn, msg)
  local send = self.conns[conn]
  -- can I put the src users's name in the mime-headers?
  local pkt = send.user.."> "..msg:payload()
  for _, recv in pairs(self.users) do
    if not(recv == send) then
      -- pcall this
      local channo = recv.channo
      local conn = recv.conn
      local chan = conn:chan(channo)
      if(chan) then
        chan:send_msg(pkt)
      end
      -- flow control?
      -- remember unacked msgno?
    end
  end
  chan:send_rpy(msg:msgno(), "ok")
end

print("register: ".."http://beep.ensembleindependant.org/profiles/chat")

vortex:profiles_register{ 
  profile = "http://beep.ensembleindependant.org/profiles/chat",
  start = function (...)
    print("start: ", ...)
    return chat:start(...)
  end,
  frame = function (...)
    print("frame: ", ...)
    return chat:frame(...)
  end,
  close = function (...)
    print("close: ", ...)
    return chat:close(...)
  end,
}

vortex:listener_new{port=44000}
vortex:listener_wait()
vortex:exit()

-- vim:ft=lua:
