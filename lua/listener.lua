require"vortex"

vortex:init()

vortex:profiles_register(
  "http://fact.aspl.es/profiles/plain_profile",
  { 
    start = function ()
      print"start..."
      return true
    end,
    close = function ()
      print"close..."
      --vortex:exit()
--[[
Uncommenting the exit above causes:

(process:7716): GLib-CRITICAL **: g_async_queue_length: assertion `g_atomic_int_get (&queue->ref_count) > 0' failed

(process:7716): GLib-CRITICAL **: g_async_queue_push: assertion `g_atomic_int_get (&queue->ref_count) > 0' failed

(process:7716): GLib-CRITICAL **: g_async_queue_length: assertion `g_atomic_int_get (&queue->ref_count) > 0' failed

]]
      return true
    end,
    frame = function (channel, connection, frame)
      print"frame..."
      print(frame:msgno())
      print(frame:payload())
      channel:send_rpy(frame:msgno(), "seen, ", frame:payload())
    end,
  }
)

vortex:listener_new("0.0.0.0", 44000)

--[[, {
  ready = function () print "ready..." end,
  accepted = function () print "accepted..." end,
}
]]

vortex:listener_wait()

print"listener wait done"

