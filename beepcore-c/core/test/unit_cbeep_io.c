/*
 * Copyright (c) 2001 Invisible Worlds, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the Blocks Public License (the
 * "License"); You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at http://www.beepcore.org/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 */

#include "CBEEPint.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

char * profiles[3] = {
  "http://first/profile",
  "http://second/profile",
  NULL
  };

void notify_lower(struct session * s, int i) {
  char * cp;
  printf("Notify lower: %d\n", i);
  cp = bll_status_text(s);
  if (cp==NULL) cp = ">>NULL<<";
  printf("  text: %s\n", cp);
  printf("  chan: %ld\n", bll_status_channel(s));
}

void notify_upper(struct session * s, long c, int i) {
  printf("Notify upper: %ld, %d\n", c, i);
}

int main() {
  struct session * sess;
  struct beep_iovec * biv;
  int total_size = 0;
  setbuf(stdout, NULL);
  printf("Starting...\n"); fflush(stdout);
  sess = bll_session_create(
    malloc, free, notify_lower, notify_upper,
    'L', "features", "localize", profiles, NULL, NULL);
  printf("FIRST CALL\n"); fflush(stdout);
  bll_session_debug_print(sess, stdout);

  biv = bll_out_buffer(sess);
  while (biv != NULL && 0 < biv->vec_count && 0 < biv->iovec[0].iov_len) {
    int size; int inx;
    for (inx = size = 0; inx < biv->vec_count; inx++)
      size += biv->iovec[inx].iov_len;
    if (25 < size) size = 25;
    total_size += size;
    bll_out_count(sess, size);
    biv = bll_out_buffer(sess);
    printf("\nAFTER %d OCTETS:\n", total_size);
    bll_session_debug_print(sess,stdout);
  }

  printf("Yes so far!\n");

  /* OK, here test the frame entry stuff. */
  {
    char * s;
    char * p;
    int i; int j;
    struct beep_iovec * x;

    s = "RPY 0 0 . 0 48\r\ncontent-type:application/xml+beep\r\n\r\n<greeting/>END\r\nSEQ 0 100 6000\r\n";
    p = s;
    j = 0;
    while (*p) {
      x = bll_in_buffer(sess);
      if (x == NULL) printf("NULL found\n");
      assert(x != NULL);
      if (1 != x->vec_count) printf("1 not found\n");
      assert(1 == x->vec_count); /* Just because I that's how it's implemented */
      i = 10;
      if (x->iovec[0].iov_len < i) i = x->iovec[0].iov_len;
      if (strlen(p) < i) i = strlen(p);
      memmove(x->iovec[0].iov_base, p, i);
      p += i; j += i;
      bll_in_count(sess, i);
      fprintf(stdout, "\nAFTER %d BYTES:\n", j);
      bll_session_debug_print(sess, stdout);
   }
 }

  biv = bll_out_buffer(sess);
  printf("\nWE SHOULD HAVE MORE NOW!\n");
  bll_session_debug_print(sess,stdout);
  while (biv != NULL && 0 < biv->vec_count && 0 < biv->iovec[0].iov_len) {
    int size; int inx;
    for (inx = size = 0; inx < biv->vec_count; inx++)
      size += biv->iovec[inx].iov_len;
    if (25 < size) size = 25;
    total_size += size;
    bll_out_count(sess, size);
    biv = bll_out_buffer(sess);
    printf("\nAFTER %d OCTETS:\n", total_size);
    bll_session_debug_print(sess,stdout);
  }

  /* Now we're going to queue three messages on channel 0, and make sure
     they go out in the right order. */

  {
    struct frame * f1;
    struct frame * f2;
    struct frame * f3;
    long ll;
    f1 = blu_frame_create(sess, 10);
    f1->channel_number = 0; f1->message_number = 1; f1->more = '*'; f1->msg_type = 'M';
    strcpy(f1->payload, "AAAAAAAAAA");
    f2 = blu_frame_create(sess, 10);
    f2->channel_number = 0; f2->message_number = 1; f2->more = '.'; f2->msg_type = 'M';
    strcpy(f2->payload, "BBBBBBBBBB");
    f3 = blu_frame_create(sess, 10);
    f3->channel_number = 0; f3->message_number = 2; f3->more = '.'; f3->msg_type = 'M';
    strcpy(f3->payload, "CCCCCCCCCC");

    ll = blu_local_window_set(sess, 0, 8000); /* Test creation of SEQ messages */
    printf("PREVIOUS WINDOW WAS %ld\n", ll);
    blu_frame_send(f1);
    blu_frame_send(f3);
    blu_frame_send(f2);

    printf("\n\nHERE WE GO WITH OUT OF ORDER DATA\n");
    bll_session_debug_print(sess,stdout);

    biv = bll_out_buffer(sess);
    while (biv != NULL && 0 < biv->vec_count && 0 < biv->iovec[0].iov_len) {
      int size; int inx;
      for (inx = size = 0; inx < biv->vec_count; inx++)
        size += biv->iovec[inx].iov_len;
      if (5 < size) size = 5;
      total_size += size;
      bll_out_count(sess, size);
      biv = bll_out_buffer(sess);
      printf("\nAFTER %d OCTETS:\n", total_size);
      bll_session_debug_print(sess,stdout);
    }

    printf("Was it good for you?\n");

  }
  return 0;
}

