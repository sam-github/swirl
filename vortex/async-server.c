#include <vortex.h>

#define PROFILE "http://example.com/beep/delayed"
/*
MSG payload is msgid

server waits a random amount of time, and sends reply.

RPY payload is "msgid delay", where msgid is the msgid of the request being
responded to, and delay is how long the server delayed before sending the
reply.

Actual code might put msgid in a MIME header, and the payload would be the
request information. Or the request would be xml encoded, and include a msgid.

There are lots of variations of this, none of them requiring extending the BEEP
protocol.
*/

VortexQueue* msgno_queue;

void frame_received(VortexChannel* chan, VortexConnection* conn, VortexFrame* frame, void*v)
{
  int msgno = vortex_frame_get_msgno (frame);
  int delay = (rand() % 30) + 1;
  const char* msgid = vortex_frame_get_payload(frame);
  char str[128];

  vortex_queue_push(msgno_queue, INT_TO_PTR(msgno));

  printf("msgno %d msgid %s delay %d\n", msgno, msgid, delay);

  sleep(delay);

  msgno = PTR_TO_INT(vortex_queue_pop(msgno_queue));

  sprintf(str, "%s %d", msgid, delay);

  vortex_channel_send_rpy(chan, str, strlen(str), msgno);
}

const char USAGE[] = "usage: %s <port>";

int main(int argc, char ** argv)
{
  if(!argv[1]) {
    printf("%s\n", USAGE);
    return 1;
  }

  msgno_queue = vortex_queue_new();

  vortex_init();

  vortex_log_enable(1);

  vortex_profiles_register(PROFILE, NULL, NULL, NULL, NULL, frame_received, NULL);

  vortex_listener_new("0.0.0.0", argv[1], NULL, NULL);

  vortex_listener_wait();

  vortex_exit();

  return 0;
}

