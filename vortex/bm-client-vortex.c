#include <assert.h>
#include <stdlib.h>
#include <vortex.h>

#define NULL_PROFILE "http://example.com/beep/null"

char* BUF = NULL;
size_t SIZE = 5000;
time_t TIME = 5;
int SEQ = 2;
int PAR = 1;

time_t t0 = 0;
time_t t = 0;
size_t size = 0;

void send_msg(VortexChannel* channel)
{
  int ok = vortex_channel_send_msg(channel, BUF, SIZE, NULL);
  assert(ok);
}

void on_start(int chno, VortexChannel *chan, void* v)
{
  int i;

  if (!chan) {
    fprintf(stderr, "Unable to create a channel..\n");
    exit(1);
  }
  vortex_channel_set_complete_flag(chan, 1);
  vortex_channel_set_automatic_mime(chan, 2);

  if(!t0)
    t0 = time(NULL);

  for(i = 0; i < SEQ; i++) {
    send_msg(chan);
  }
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
    printf("seq\t2\tpar\t1\tb\t%ld\tt\t%ld\trate\t%g\tkB/s\n", size, t, size / 1000.0 / t);
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
  int opt;
  int i;

  vortex_init ();

  while((opt = getopt(argc, argv, "p:h:T:Z:S:P:")) != -1)
  {
    switch(opt)
    {
      case 'p':
	port = optarg;
	break;
      case 'h':
	host = optarg;
	break;
      case 'T':
	TIME = atoi(optarg);
	break;
      case 'Z':
	SIZE = atoi(optarg);
	break;
      case 'S':
	SEQ = atoi(optarg);
	break;
      case 'P':
	PAR = atoi(optarg);
	break;
      default:
	return 1;
    }
  }

  printf("options: port=%s host=%s time=%d size=%d seq=%d par=%d",
      port, host, (int)TIME, (int)SIZE, SEQ, PAR
      );

  BUF = malloc(SIZE);
  assert(BUF);
  memset(BUF, 'x', SIZE);

  connection = vortex_connection_new(host, port, NULL, NULL);

  if (!vortex_connection_is_ok(connection, false)) {
    fprintf(stderr, "Unable to connect remote server, error was: %s\n",
	vortex_connection_get_message(connection));
    return 1;
  }

  for(i = 0; i < PAR; i++) {
    vortex_channel_new(connection, 0,
	profile,
	NULL, NULL, /* no close handling */
	on_frame, NULL,
	on_start, NULL
	);
  }

  // this is awful, but vortex appears to have no API for his
  { char c; read(0, &c, 1); }

  vortex_exit ();

  return 0 ;         
}

