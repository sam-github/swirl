#include <vortex.h>

#include <assert.h>

#define NULL_PROFILE "http://example.com/beep/null"

#define CHANMAX 20
int msgsz[CHANMAX];
// Hopefully all channels are less CHANMAX, since I'm not building a dynamic
// data structure for a trivial benchmark app.

void frame_received(VortexChannel* chan, VortexConnection* conn, VortexFrame* frame, void*v)
{
  int chno = vortex_channel_get_number (chan);
  char str[] = "1000000000000";

  msgsz[chno] +=  vortex_frame_get_content_size(frame);

  if(vortex_frame_get_more_flag(frame))
    return;

  sprintf(str, "%d", msgsz[chno]);

  vortex_channel_send_rpy(chan, str, strlen(str), vortex_frame_get_msgno (frame));

  msgsz[chno] = 0;
}

bool start_channel(int chno, VortexConnection* conn, void* v)
{
  VortexChannel* chan = vortex_connection_get_channel(conn, chno);
  printf("start %d\n", chno);
  assert(chno < CHANMAX);
  msgsz[chno] = 0;
  vortex_channel_set_automatic_mime(chan, 2);
  return true;
}

bool close_channel(int chno, VortexConnection* conn, void* v)
{
  printf("close %d\n", chno);
  return true;
}

bool on_accepted(VortexConnection* conn, void* v)
{
  printf ("accepted from: %s:%s\n",
      vortex_connection_get_host (conn),
      vortex_connection_get_port (conn));
  return true;
}

const char USAGE[] = "usage: %s <port>\n";

int main(int argc, char ** argv)
{
  if(argc != 2) {
    printf(USAGE, argv[0]);
    return 1;
  }

  vortex_init();

  /*vortex_log_enable(1);*/
  /*vortex_log2_enable(1);*/

  vortex_profiles_register(NULL_PROFILE,
      start_channel, NULL,
      close_channel, NULL,
      frame_received, NULL
      );

  vortex_listener_new("0.0.0.0", argv[1], NULL, NULL);
  vortex_listener_set_on_connection_accepted(on_accepted, NULL);
  vortex_listener_wait();
  vortex_exit();

  return 0;
}

