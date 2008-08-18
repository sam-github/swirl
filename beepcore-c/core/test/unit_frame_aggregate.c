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
#include "../generic/CBEEPint.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <strings.h>

#include "../generic/channel_0.h"
#include "../generic/base64.h"

#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif


char * rawstr = "Once upon a midnight dreary as I pondered weak and weary I heard a rapping tap tap tapping at my parlor door.";
char * codedstr = "T25jZSB1cG9uIGEgbWlkbmlnaHQgZHJlYXJ5IGFzIEkgcG9uZGVyZWQgd2VhayBhbmQgd2Vhcnkg\r\nSSBoZWFyZCBhIHJhcHBpbmcgdGFwIHRhcCB0YXBwaW5nIGF0IG15IHBhcmxvciBkb29yLg=="; 

int main () {

  struct frame * resultptr;

  struct frame myframe1 = {NULL, NULL, NULL,
			   'M', 0, 0,
                           0, '+', 0, 
			   40, rawstr};

  struct frame myframe2 = {NULL, NULL, NULL,
			   'M', 0, 0,
                           0, '.', 0,
			   strlen(rawstr) - 40, &(rawstr[40])};

  struct frame myframe3 = {NULL, NULL, NULL,
			   'M', 0, 0,
                           0, '.', 0,
			   strlen(rawstr), rawstr};

  struct session mysession;

  memset(&mysession, 0, sizeof(struct session));
  
  myframe1.session = myframe2.session = myframe3.session = &mysession;
  myframe1.next_in_message = myframe1.next_on_channel = &myframe2;
  

  mysession.malloc = malloc;
  mysession.free   = free;

  printf("identity test (single frame copy): ");
  resultptr=blu_frame_aggregate(&mysession, &myframe3);
  if (resultptr) {
    printf("got a frame: ");
    if (0 == bcmp(resultptr, &myframe3, sizeof(myframe3) - sizeof(char *)))
      printf("failed compare 1\n");
    else if (0 == bcmp(resultptr->payload, myframe3.payload, strlen(myframe3.payload)))
      printf("failed compare 2\n");
    else 
      printf("passed compare\n");      
  } else {
    printf("FAILED\n");
  }

  if (resultptr) {
    free(resultptr);
    resultptr = NULL;
  }

  printf("aggregate test: ");
  resultptr=blu_frame_aggregate(&mysession, &myframe1);
  if (resultptr && 
      resultptr->session == myframe1.session &&
      NULL == resultptr->next_in_message &&
      NULL == resultptr->next_on_channel &&
      resultptr->msg_type == myframe1.msg_type &&
      '.' == resultptr->more &&
      resultptr->channel_number == myframe1.channel_number &&
      resultptr->message_number == myframe1.message_number &&
      resultptr->answer_number == myframe1.answer_number &&
      0 == bcmp(resultptr->payload, myframe1.payload, strlen(myframe1.payload))) {
    printf("passed\n");
  } else {
    printf("FAILED'\n");
  }

  if (resultptr) {
    free(resultptr);
    resultptr = NULL;
  }

  printf("negative test: ");
  myframe1.payload = codedstr;
  resultptr=blu_frame_aggregate(&mysession, &myframe1);
  if (resultptr && 
      resultptr->session == myframe1.session &&
      NULL == resultptr->next_in_message &&
      NULL == resultptr->next_on_channel &&
      resultptr->msg_type == myframe1.msg_type &&
      '.' == resultptr->more &&
      resultptr->channel_number == myframe1.channel_number &&
      resultptr->message_number == myframe1.message_number &&
      resultptr->answer_number == myframe1.answer_number &&
      0 == strcmp(resultptr->payload, myframe1.payload)) {
    printf("FAILED'\n");
  } else {
    printf("passed\n");
  }

  if (resultptr) {
    free(resultptr);
    resultptr = NULL;
  }

  exit(0);
}

