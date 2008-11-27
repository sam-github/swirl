#include <vortex.h>
#include <assert.h>

#define PROFILE "http://example.com/beep/delayed"

int msgid = 0;

void send_msg(VortexChannel* channel)
{
  int ok = vortex_channel_send_msgv(channel, 0, "%d", ++msgid);
  assert(ok);
}

void on_frame(VortexChannel* channel, VortexConnection* conn, VortexFrame* frame, void* v)
{
  int msgno = vortex_frame_get_msgno(frame);
  const char* content = vortex_frame_get_payload(frame);

  printf("(msgno %d) %s\n", msgno, content);

  send_msg(channel);
}

int main (int argc, char ** argv)
{
  VortexConnection * connection = NULL;
  VortexChannel * channel = NULL;

  vortex_init ();

  connection = vortex_connection_new(argv[1], argv[2], NULL, NULL);

  if (!vortex_connection_is_ok(connection, false)) {
    fprintf(stderr, "Unable to connect remote server, error was: %s\n",
	vortex_connection_get_message(connection));
    return 1;
  }

  channel = vortex_channel_new(connection, 0,
      PROFILE,
      NULL, NULL, /* no close handling */
      on_frame, NULL,
      NULL, NULL /* no async channel creation */
      );

  if (channel == NULL) {
    fprintf(stderr, "Unable to create the channel..\n");
    return 1;
  }

  printf(".. send msg\n");

  send_msg(channel);
  send_msg(channel);
  send_msg(channel);
  send_msg(channel);
  send_msg(channel);

  {
    char c;
    read(0, &c, 1);
  }
  vortex_exit ();

  return 0 ;         
}

