#include <vortex.h>

#define PLAIN_PROFILE "http://beep.ensembleindependant.org/profiles/plain_profile"

static
void on_frame_received (VortexChannel    * channel,
		     VortexConnection * connection,
		     VortexFrame      * frame,
		     gpointer           user_data)
{
  write(1, vortex_frame_get_payload(frame), vortex_frame_get_payload_size(frame));

  vortex_channel_send_rpy(channel, "", 0, vortex_frame_get_msgno(frame));
}

gint main (gint argc, gchar ** argv)
{
  VortexConnection * connection = NULL;
  VortexChannel    * channel = NULL;
  VortexFrame      * frame = NULL;
  WaitReplyData    * wait_reply = NULL;
  gint               msg_no = NULL;
  char input[1024];

  vortex_init ();

  vortex_color_log_enable(TRUE);

  g_print ("! connecting to localhost:44000\n");
  connection = vortex_connection_new ("localhost", "44000", NULL, NULL);
  if (!vortex_connection_is_ok (connection, FALSE)) {
    g_print ("Unable to connect remote server, error was: %s\n",
        vortex_connection_get_message (connection));
    goto end;
  }
  g_print ("! connected\n");

  channel = vortex_channel_new_full (
      connection,
      0,
      NULL, // serverName
      "http://beep.ensembleindependant.org/profiles/chat",
      EncodingNone, "sam/c", 3,
      NULL, NULL, // no close handling
      on_frame_received, NULL, // no frame receive async handling
      NULL, NULL  // no async channel creation
      );
  if (channel == NULL) {
    g_print ("Unable to create the channel..\n");
    goto end;
  }


  while(fgets(input, sizeof(input), stdin))
  {
    g_assert(!wait_reply);

    wait_reply = vortex_channel_create_wait_reply ();

    g_assert(wait_reply);

    if(input[strlen(input)-1] == '\n')
      input[strlen(input)-1] = 0;

    g_print("! send <%s>\n", input);

    if(!vortex_channel_send_msg_and_wait(channel, input, strlen (input), &msg_no, wait_reply)) {
      g_print ("! send failed\n");
      vortex_channel_free_wait_reply (wait_reply);
      vortex_channel_close (channel, NULL);
      goto end;
    }

    frame = vortex_channel_wait_reply(channel, msg_no, wait_reply);
    if(frame == NULL) {
      g_print ("! wait_reply failed\n");
      vortex_channel_close (channel, NULL);
      goto end;
    }
    wait_reply = NULL;

    g_print ("! reply %d <%s>\n", 
        vortex_frame_get_payload_size (frame), 
        vortex_frame_get_payload (frame));
  }

end:				      
  vortex_connection_close (connection);
  vortex_exit ();
  return 0 ;	      
}

