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

#include "CBEEP.h"
#include "CBEEPint.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

extern void bll_frame_debug_print(struct frame *, FILE *);

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
  setbuf(stdout, NULL);
  printf("Starting...\n"); fflush(stdout);
  sess = bll_session_create(
    malloc, free, notify_lower, notify_upper,
    'L', "features", "localize", profiles, NULL, NULL);
  printf("FIRST CALL\n"); fflush(stdout);
  bll_session_debug_print(sess, stdout);

  {

    /* Now we queue six frames incoming, to test the query routine */
    struct frame * f1 = blu_frame_create(sess,10);
    struct frame * f2 = blu_frame_create(sess,10);
    struct frame * f3 = blu_frame_create(sess,10);
    struct frame * f4 = blu_frame_create(sess,10);
    struct frame * f5 = blu_frame_create(sess,10);
    struct frame * f6 = blu_frame_create(sess,10);

    struct frame * answer;

    printf("Begin now.\n");
    strcpy(f1->payload, "one");
    strcpy(f2->payload, "two");
    strcpy(f3->payload, "three");
    strcpy(f4->payload, "four");
    strcpy(f5->payload, "five");
    strcpy(f6->payload, "six");

    f1->next_on_channel = f2;
    f2->next_on_channel = f3;
    f3->next_on_channel = f4;
    f4->next_on_channel = f5;
    f5->next_on_channel = f6;

    f1->msg_type = 'M'; f1->message_number = 3; f1->more = '*';
    f2->msg_type = 'M'; f2->message_number = 3; f2->more = '.';
    f3->msg_type = 'M'; f3->message_number = 7; f3->more = '*';
    f4->msg_type = 'M'; f4->message_number = 9; f4->more = '.';
    f5->msg_type = 'M'; f5->message_number = 7; f5->more = '.';
    f6->msg_type = 'M'; f6->message_number = 9; f6->more = '.';
    f1->channel_number = 0;
    f2->channel_number = 0;
    f3->channel_number = 0;
    f4->channel_number = 0;
    f5->channel_number = 0;
    f6->channel_number = 0;

    sess->channel->in_frame = f1; /* Zap! */
    printf("\nFIRST QUERY TEST\n");
    bll_session_debug_print(sess,stdout);
    printf("Good so far...\n");
    answer = blu_frame_query(sess, 0, ' ', -1, -1, '*');
    if (answer != f1) printf("Wrong at 1\n");
    bll_session_debug_print(sess,stdout);
    bll_frame_debug_print(answer,stdout);
    if (answer->next_on_channel != NULL) printf("Wrong at 2\n");
    if (sess->channel->in_frame != f2) printf("Wrong at 3\n");
    printf("If you didn't see \"wrong at ...\" then it worked.\n");

    f1->next_on_channel = f2;   f1->next_in_message = NULL;
    f2->next_on_channel = f3;   f2->next_in_message = NULL;
    f3->next_on_channel = f4;   f3->next_in_message = NULL;
    f4->next_on_channel = f5;   f4->next_in_message = NULL;
    f5->next_on_channel = f6;   f5->next_in_message = NULL;
    f6->next_on_channel = NULL; f6->next_in_message = NULL;
    sess->channel->in_frame = f1; /* Zap! */

    printf("\nSECOND QUERY TEST\n");
    answer = blu_frame_query(sess, 0, 'M', -1, -1, '.');
    bll_session_debug_print(sess,stdout);
    printf("Answer:\n");
    bll_frame_debug_print(answer,stdout);
    printf("f2:\n");
    bll_frame_debug_print(f2,stdout);
    if (answer != f1 ||
	f1->next_in_message != f2 ||
	sess->channel->in_frame != f1) printf("Wrong at 4\n");
    if (f1->next_on_channel != NULL || f2->next_on_channel != NULL ||
	f2->next_in_message != NULL) printf("Wrong at 5\n");
    printf("If no \"wrong at\" above, it seems to have worked.\n");

    printf("\nTHIRD QUERY TEST\n");
    f1->next_on_channel = f2;   f1->next_in_message = NULL;
    f2->next_on_channel = f3;   f2->next_in_message = NULL;
    f3->next_on_channel = f4;   f3->next_in_message = NULL;
    f4->next_on_channel = f5;   f4->next_in_message = NULL;
    f5->next_on_channel = f6;   f5->next_in_message = NULL;
    f6->next_on_channel = NULL; f6->next_in_message = NULL;
    sess->channel->in_frame = f1; /* Zap! */

    answer = blu_frame_query(sess, 0, 'M', 7, -1, '.');
    bll_session_debug_print(sess,stdout);
    printf("Answer:\n");
    bll_frame_debug_print(answer,stdout);
    printf("f5:\n");
    bll_frame_debug_print(f5,stdout);
    if (answer != f3 ||
	f3->next_in_message != f5 ||
	sess->channel->in_frame != f1) printf("Wrong at 6\n");
    if (f3->next_on_channel != NULL || f5->next_on_channel != NULL ||
	f5->next_in_message != NULL) printf("Wrong at 7\n");
    if (f1->next_on_channel != f2 || f2->next_on_channel != f4 ||
	f4->next_on_channel != f6 || f6->next_on_channel != NULL)
	  printf("Wrong at 8\n");
    printf("If no \"wrong at\" above, it seems to have worked.\n");

    printf("\nFOURTH QUERY TEST\n");
    f1->next_on_channel = f2;   f1->next_in_message = NULL;
    f2->next_on_channel = f3;   f2->next_in_message = NULL;
    f3->next_on_channel = f4;   f3->next_in_message = NULL;
    f4->next_on_channel = f5;   f4->next_in_message = NULL;
    f5->next_on_channel = f6;   f5->next_in_message = NULL;
    f6->next_on_channel = NULL; f6->next_in_message = NULL;
    sess->channel->in_frame = f1; /* Zap! */

    /* Note there are two complete #9 messages. Test we only get one */
    answer = blu_frame_query(sess, 0, 'M', 9, -1, '.');
    bll_session_debug_print(sess,stdout);
    printf("Answer:\n");
    bll_frame_debug_print(answer,stdout);
    printf("f6:\n");
    bll_frame_debug_print(f6,stdout);
    if (answer != f4 ||
	f4->next_in_message != NULL ||
	sess->channel->in_frame != f1) printf("Wrong at 9\n");
    if (f4->next_on_channel != NULL || f6->next_on_channel != NULL ||
	f6->next_in_message != NULL) printf("Wrong at 10\n");
    if (f1->next_on_channel != f2 || f2->next_on_channel != f3 ||
	f3->next_on_channel != f5 || f5->next_on_channel != f6 ||
	f6->next_on_channel != NULL)
	  printf("Wrong at 11\n");
    printf("If no \"wrong at\" above, it seems to have worked.\n");

    printf("\nFIFTH QUERY TEST\n");

    /* Note there are two complete #9 messages. 
       Test we get the other, and that end-of-list works. */
    answer = blu_frame_query(sess, 0, 'M', 9, -1, '.');
    bll_session_debug_print(sess,stdout);
    printf("Answer:\n");
    bll_frame_debug_print(answer,stdout);
    if (answer != f6 ||
	f4->next_in_message != NULL ||
	sess->channel->in_frame != f1) printf("Wrong at 12\n");
    if (f6->next_on_channel != NULL || f5->next_on_channel != NULL ||
	f6->next_in_message != NULL) printf("Wrong at 13\n");
    if (f1->next_on_channel != f2 || f2->next_on_channel != f3 ||
	f3->next_on_channel != f5 || f5->next_on_channel != NULL)
	  printf("Wrong at 14\n");
    printf("If no \"wrong at\" above, it seems to have worked.\n");


    printf("\nSIXTH QUERY TEST\n");
    f1->next_on_channel = f2;   f1->next_in_message = NULL;
    f2->next_on_channel = NULL; f2->next_in_message = NULL;
    sess->channel->in_frame = f1; /* Zap! */

    /* Check that grabbing *all* of a queue works. */
    answer = blu_frame_query(sess, 0, 'M', -1, -1, '.');
    bll_session_debug_print(sess,stdout);
    printf("Answer:\n");
    bll_frame_debug_print(answer,stdout);
    if (answer != f1 ||
	f1->next_in_message != f2 ||
	sess->channel->in_frame != NULL) printf("Wrong at 14\n");
    if (f1->next_on_channel != NULL || f2->next_on_channel != NULL)
	printf("Wrong at 15\n");
    printf("If no \"wrong at\" above, it seems to have worked.\n");

    printf("\nSEVENTH QUERY TEST\n");
    f1->next_on_channel = NULL;  f1->next_in_message = NULL;
    sess->channel->in_frame = f1; /* Zap! */

    /* Check that grabbing the only one on the queue works. */
    answer = blu_frame_query(sess, 0, 'M', -1, -1, '*');
    bll_session_debug_print(sess,stdout);
    printf("Answer:\n");
    bll_frame_debug_print(answer,stdout);
    if (answer != f1 ||
	f1->next_in_message != NULL ||
	sess->channel->in_frame != NULL) printf("Wrong at 16\n");
    printf("If no \"wrong at\" above, it seems to have worked.\n");


    printf("\nEIGTH QUERY TEST\n");
    f1->next_on_channel = NULL;  f1->next_in_message = NULL;
    sess->channel->in_frame = f1; /* Zap! */

    /* Check that missing works */
    answer = blu_frame_query(sess, 0, 'M', -1, -1, '.');
    bll_session_debug_print(sess,stdout);
    printf("Answer:\n");
    bll_frame_debug_print(answer,stdout);
    if (answer != NULL ||
	f1->next_in_message != NULL ||
	f1->next_on_channel != NULL ||
	sess->channel->in_frame != f1) printf("Wrong at 17\n");
    printf("If no \"wrong at\" above, it seems to have worked.\n");

  }

  return 0;
}

