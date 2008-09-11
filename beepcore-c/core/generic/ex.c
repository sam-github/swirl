#include "CBEEP.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void x_notify_lower(struct session* s, int o)
{
  printf("> notify_lower %d\n", o);
}

void x_notify_upper( struct session * s, long c, int o)
{
  printf("> notify_upper %d #%ld\n", o, c);
}

int main(int argc, char* argv[])
{
  char* profiles[] = { 0 };
  struct session* s = bll_session_create(
      malloc,
      free,
      x_notify_lower,
      x_notify_upper,
      'I', // initiator
      0, // features
      0, // localize
      profiles, // profiles
      0, // error,
      0 // user_data
      );

  printf("s=%p\n", s);

  if(s) {
    //bll_session_debug_print(s, stdout);
  }

  struct beep_iovec* iov = bll_out_buffer(s);

  printf("> [\n");
  ssize_t sz = writev(1, iov->iovec, iov->vec_count);
  printf("\n]\n");
  if(sz < 0) {
    fprintf(stderr, "writev failed [%d] %s\n", errno, strerror(errno));
  }
  bll_out_count(s, sz);

  return 0;
}

