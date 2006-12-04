io.stdout:setvbuf"line"

CHAT = "http://example.com/beep"

USER = 'NAME'

require"vortex"

-- vortex:log_enable"yes"

vortex:profiles_register{
  profile = CHAT,
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

        local i = 0
        while true do
          local s = "msg "..i
          print("> "..s)
          local ok = chan:send_msg(s)
          print("? "..ok)
          if not ok then
            return
          end
          i = i + 1
        end
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
