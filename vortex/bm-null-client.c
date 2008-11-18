#include <vortex.h>

#include <assert.h>
//#include <stdlib.h>
//#include <stdio.h>

#define NULL_PROFILE "http://example.com/beep/null"

char* BUF = NULL;
size_t BUFSZ = 5000;
time_t TIME = 5;
time_t t0 = 0;
time_t t = 0;
size_t size = 0;

void send_msg(VortexChannel* channel)
{
  int ok = vortex_channel_send_msg(channel, BUF, BUFSZ, NULL);
  assert(ok);
}

void on_frame(VortexChannel* channel, VortexConnection* conn, VortexFrame* frame, void* v)
{
  const char* content = vortex_frame_get_content(frame);
  int sz = atoi(content);

  (void) v;

  assert(sz > 0);

  size += sz;

  t = time(NULL) - t0;

  if(t >= TIME) {
    printf("seq\t2\tpar\t1\tb\t%d\tt\t%ld\trate\t%g\tkB/s\n", size, t, size / 1000.0 / t);
    exit(0);
  }

  send_msg(channel);
}

int main (int argc, char ** argv)
{
  const char* host = "localhost";
  const char* port = NULL;
  const char* profile = NULL_PROFILE;
  VortexConnection * connection = NULL;
  VortexChannel * channel = NULL;

  vortex_init ();

  switch(argc)
  {
    case 4:
      TIME = atoi(argv[3]);
    case 3:
      host = argv[2];
    case 2:
      port = argv[1];
      break;
    default:
      fprintf(stderr, "usage: %s port [host] [time]\n", argv[0]);
      return 0;
  }

  BUF = malloc(BUFSZ);
  assert(BUF);
  memset(BUF, 'x', BUFSZ);

  printf ("connecting to %s:%s, profile=%s\n", host, port, profile);

  connection = vortex_connection_new(host, port, NULL, NULL);

  if (!vortex_connection_is_ok(connection, false)) {
    fprintf(stderr, "Unable to connect remote server, error was: %s\n",
	vortex_connection_get_message(connection));
    return 1;
  }

  printf(".. get channel\n");

  channel = vortex_channel_new(connection, 0,
      profile,
      NULL, NULL, /* no close handling */
      on_frame, NULL,
      NULL, NULL /* no async channel creation */
      );

  if (channel == NULL) {
    fprintf(stderr, "Unable to create the channel..\n");
    return 1;
  }

  vortex_channel_set_complete_flag(channel, 1);
  vortex_channel_set_automatic_mime(channel, 2);

  printf(".. send msg\n");

  t0 = time(NULL);

  send_msg(channel);
  send_msg(channel);

  {
    char c;
    read(0, &c, 1);
  }
  vortex_exit ();

  return 0 ;         
}

