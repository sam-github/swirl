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
    total_size += size;
    bll_out_count(sess, size);
    biv = bll_out_buffer(sess);
    printf("\nAFTER %d OCTETS:\n", total_size);
    bll_session_debug_print(sess,stdout);
  }

  printf("GREETING CLEANED OUT - START SHORTFRAME TEST\n");

  /* OK, here test the frame entry stuff for a really short frame. */
  {
    char * s;
    char * p;
    int i; int j;
    struct beep_iovec * x;

    s = "SEQ 0 100 5000\r\nMSG 0 9 . 0 11\r\n<greeting/>END\r\nSEQ 0 100 6000\r\n";
    p = s;
    j = 0;
    while (*p) {
      x = bll_in_buffer(sess);
      if (x == NULL) printf("NULL found\n");
      assert(x != NULL);
      if (1 != x->vec_count) printf("1 not found\n");
      assert(1 == x->vec_count); /* Just because I that's how it's implemented */
      i = strlen(s);
      if (x->iovec[0].iov_len < i) i = x->iovec[0].iov_len;
      if (strlen(p) < i) i = strlen(p);
      memmove(x->iovec[0].iov_base, p, i);
      p += i; j += i;
      bll_in_count(sess, i);
      fprintf(stdout, "\nAFTER %d BYTES:\n", j);
      bll_session_debug_print(sess, stdout);
    }
  }

  printf("START EMPTY FRAME TEST\n");

  /* OK, here test the frame entry stuff for an empty frame. */
  {
    char * s;
    char * p;
    int i; int j;
    struct beep_iovec * x;

    s = "SEQ 0 100 7000\r\nMSG 0 10 . 11 0\r\nEND\r\nSEQ 0 100 8000\r\n";
    p = s;
    j = 0;
    while (*p) {
      x = bll_in_buffer(sess);
      if (x == NULL) printf("NULL found\n");
      assert(x != NULL);
      if (1 != x->vec_count) printf("1 not found\n");
      assert(1 == x->vec_count); /* Just because I that's how it's implemented */
      i = strlen(s);
      if (x->iovec[0].iov_len < i) i = x->iovec[0].iov_len;
      if (strlen(p) < i) i = strlen(p);
      memmove(x->iovec[0].iov_base, p, i);
      p += i; j += i;
      bll_in_count(sess, i);
      fprintf(stdout, "\nAFTER %d BYTES:\n", j);
      bll_session_debug_print(sess, stdout);
    }
  }

  printf("ALL DONE!\n");

  return 0;
}

