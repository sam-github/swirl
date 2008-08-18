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
/*
 * unit_channel_0.c
 *
 * Unit testing code for channel_0.c 
 *
 */
const char * __unit_channel_0_c__ = "$Id: unit_channel_0.c,v 1.4 2002/01/01 00:25:14 mrose Exp $";

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

#include "beep_test_msg.h"

char * b64_rawstr = "Once upon a midnight dreary";
char * b64_codedstr = "AQEBT25jZSB1cG9uIGEgbWlkbmlnaHQgZHJlYXJ5"; 

int chan0_base64_isneeded(struct profile *);

/*
 * Test functions
 */
int test_parse (struct session * session) {
  
  int i = 0;
  int verbose = 0;
  char readbuf[256];
  struct chan0_msg * resultchan0;
  struct profile * resultprofile;

  struct frame myframe1 = {NULL, NULL, NULL,
			   beep_msgs[0].frametype, 0, 0,
                           0, '+', 0, 
			   40, beep_msgs[0].testdata};

  struct frame myframe2 = {NULL, NULL, NULL, 
 			   beep_msgs[0].frametype, 0, 0, 
			   0, '.', 0,
			   strlen(beep_msgs[0].testdata) - 40, &(beep_msgs[0].testdata[40])}; 

  struct frame myframe3 = {NULL, NULL, NULL, 
 			   beep_msgs[0].frametype, 0, 0, 
			   0, '.', 0,
			   strlen(beep_msgs[0].testdata), beep_msgs[0].testdata}; 

  
  myframe1.session = myframe2.session = myframe3.session = session;
  myframe1.next_in_message = myframe1.next_on_channel = &myframe2;
  

  printf("Skip N tests (enter 0 for verbose output): ");
  fgets(readbuf, 255, stdin);
  i = atoi(readbuf);

  verbose = (int)(readbuf[0] == '0');

  for (; beep_msgs[i].testdata; i++) {
    myframe3.size = strlen(beep_msgs[i].testdata);
    myframe3.payload = beep_msgs[i].testdata;
    myframe3.msg_type = beep_msgs[i].frametype;
    printf("%4d) %-40s ", i, beep_msgs[i].testname);
  
    resultchan0=chan0_msg_parse(session, &myframe3);
    if (resultchan0) {
      printf("passed\n");
      /* session->free(resultchan0); */
      if (verbose) {
	printf("\n\nInput-->  %s\n\n", beep_msgs[i].testdata);
	printf("chan0->channel_number:\t%ld\n", resultchan0->channel_number);
	printf("chan0->message_number:\t%ld\n", resultchan0->message_number);
	printf("chan0->op_ic:\t\t%c\n", resultchan0->op_ic);
	printf("chan0->op_sc:\t\t%c\n", resultchan0->op_sc);
	printf("chan0->profileC:\t%ld\n", resultchan0->profileC);
	printf("chan0->features:\t%s\n", resultchan0->features);
	printf("chan0->localize:\t%s\n", resultchan0->localize);
	printf("chan0->server_name:\t%s\n", resultchan0->server_name);

	resultprofile = resultchan0->profile;
	while (resultprofile) {
	  printf("\n");
	  printf("\tprofile->uri:\t\t\t%s\n", resultprofile->uri);
	  printf("\tprofile->piggyback_length:\t%d\n", resultprofile->piggyback_length);
	  if (resultprofile->piggyback)
	    printf("\tstrlen(profile->piggyback):\t%d\n", strlen(resultprofile->piggyback));
	  printf("\tprofile->piggyback:\t\t%s\n", resultprofile->piggyback);
	  if (resultprofile->encoding) {
	    printf("\tprofile->encoding:\t\t%c\n", resultprofile->encoding);
	  } else {
	    printf("\tprofile->encoding:\t\t\\0\n");
	  }
	  resultprofile = resultprofile->next;
	}
	printf("\n");
      }
    } else {
      printf("FAILED'\n");
      return(1);
    }
  }

  return(0);
}


/*
 * test_aggregate
 *
 * run tests on the frame_aggregate function
 */
int test_aggregate(struct session * session) {

  struct frame * resultptr;

  struct frame myframe1 = {NULL, NULL, NULL,
			   beep_msgs[0].frametype, 0, 0,
                           0, '+', 0,
			   40, beep_msgs[0].testdata};

  /*
  struct frame myframe2 = {NULL, NULL, NULL, 
 			   beep_msgs[0].frametype, 0, 0, 
			   0, '.', 0,
			   strlen(beep_msgs[0].testdata) - 40, &(beep_msgs[0].testdata[40])}; 

  struct frame myframe3 = {NULL, NULL, NULL, 
 			   beep_msgs[0].frametype, 0, 0, 
			   0, '.', 0,
			   strlen(beep_msgs[0].testdata), beep_msgs[0].testdata}; 
  */

  printf("aggregate test: ");
  resultptr=blu_frame_aggregate(session, &myframe1); 

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
    return(1);
  }

  printf("negative test: ");
  myframe1.payload = beep_msgs[0].testdata;
  resultptr=blu_frame_aggregate(session, &myframe1);
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
    printf("FAILED'\n");
    return(1);
  } else {
    printf("passed\n");
  }

  return(0);
}

/*
 * test_fmt
 *
 * routine for testing the chan0_fmt routines.
 */
int test_fmt(struct session * session) {

  int retcode = 0;
  struct frame * retframe;
  struct chan0_msg msg;
  struct diagnostic err;
  struct profile profile1, profile2;

  msg.channel_number = 0;
  msg.message_number = 10;
  msg.op_ic          = 'c';
  msg.op_sc          = 'c';
  msg.profile        = NULL;
  msg.error          = NULL;
  msg.features       = NULL;
  msg.localize       = NULL;
  msg.server_name     = NULL;

  profile1.next      = NULL;
  profile1.uri       = "http://iana.org/beep/SASL/OTP";
  profile1.piggyback = NULL;
  profile1.piggyback_length = 0;

  profile2.next      = &profile1;
  profile2.uri       = "http://iana.org/beep/NULL/ECHO";
  profile2.piggyback = NULL;
  profile2.piggyback_length = 0;


  err.code           = 451;
  err.message        = "Egregious error message.";
  err.lang           = "eng/us";

  /* greet confirm -- empty */
  msg.op_ic          = 'c';
  msg.op_sc          = 'g';
  msg.channel_number = 11;
  msg.profile        = NULL;
  printf("chan0_msg_fmt: greeting confirm (empty): ");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("PASSED: %s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }
  msg.profile        = NULL;
  msg.channel_number = 0;

  /* greet confirm */
  msg.op_ic          = 'c';
  msg.op_sc          = 'g';
  msg.channel_number = 11;
  msg.profile        = &profile1;
  printf("chan0_msg_fmt: greeting confirm: ");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("PASSED: %s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }
  msg.profile        = NULL;
  msg.channel_number = 0;

  /* start request*/
  msg.op_ic          = 'i';
  msg.op_sc          = 's';
  msg.channel_number = 11;
  msg.profile        = &profile1;
  printf("chan0_msg_fmt: start: ");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("PASSED: %s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }
  msg.profile        = NULL;
  msg.channel_number = 0;

  /* start request with entities needing normalization*/
  msg.op_ic          = 'i';
  msg.op_sc          = 's';
  msg.channel_number = 11;
  msg.profile        = &profile1;

  profile1.uri       = "http://iana.org/beep/SASL/OTP?><@##%#%'";
  msg.server_name     = "<jan&dean>";

  profile1.piggyback = b64_rawstr;
  profile1.piggyback_length = strlen(b64_rawstr);

  printf("chan0_msg_fmt: start : entities to do:");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("PASSED: %s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }

  profile1.uri       = "http://iana.org/beep/SASL/OTP";

  profile1.piggyback = NULL;
  profile1.piggyback_length = 0;
  msg.profile        = NULL;
  msg.channel_number = 0;

  /* start request with piggyback needing base64*/
  msg.op_ic          = 'i';
  msg.op_sc          = 's';
  msg.channel_number = 11;
  msg.profile        = &profile1;

  profile1.piggyback = b64_rawstr;
  profile1.piggyback_length = strlen(b64_rawstr);

  printf("chan0_msg_fmt: start b64 piggyback: ");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("PASSED: %s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }

  profile1.piggyback = NULL;
  profile1.piggyback_length = 0;
  msg.profile        = NULL;
  msg.channel_number = 0;

  /* start request -- 2 profiles*/
  msg.op_ic          = 'i';
  msg.op_sc          = 's';
  msg.channel_number = 11;
  msg.profile        = &profile2;
  printf("chan0_msg_fmt: start (2 profiles): ");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("PASSED: %s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }
  msg.profile        = NULL;
  msg.channel_number = 0;

  /* start confirm */
  msg.op_ic          = 'c';
  msg.op_sc          = 's';
  msg.channel_number = 11;
  msg.profile        = &profile1;
  printf("chan0_msg_fmt: start confirm: ");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("PASSED: %s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }
  msg.profile        = NULL;
  msg.channel_number = 0;

 /* start confirm negative test */
  msg.op_ic          = 'c';
  msg.op_sc          = 's';
  msg.channel_number = 11;
  msg.profile        = &profile2;
  printf("chan0_msg_fmt: start confirm negative test (too many profiles): ");
  if ((!(retframe = chan0_msg_fmt(session, &msg)))) {
    printf("PASSED: \n");
  } else {
    blu_frame_destroy(retframe);
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }
  msg.profile        = NULL;
  msg.channel_number = 0;

  /* close request*/
  msg.op_ic          = 'i';
  msg.op_sc          = 'c';
  msg.error          = &err;
  printf("chan0_msg_fmt: close: ");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("%s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }
  msg.error          = NULL;

  /* ok message for close */
  msg.op_ic          = 'c';
  msg.op_sc          = 'c';
  printf("chan0_msg_fmt: close ok: ");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("%s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }

  /* error frame return for start or close */
  msg.op_ic          = 'c';
  msg.op_sc          = 'e';
  msg.error          = &err;
  printf("chan0_msg_fmt: error: ");
  if ((retframe = chan0_msg_fmt(session, &msg))) {
    printf("%s\n", retframe->payload);
    blu_frame_destroy(retframe);
  } else {
    printf("FAIL\n");
    retcode = 1;
    goto cleanup;
  }
  msg.error          = NULL;

 cleanup:
  return(retcode);
}


/*
 * test_base64_isneeded
 */
int test_base64_isneeded() {
  struct profile myprofile2 = {NULL, "", 
			       "Now is the time for all good men.", 32, ' '};

  struct profile myprofile1 = {&myprofile2, "", 
			       "Now is the time for all good men.", 32, ' '};

  myprofile1.piggyback_length = strlen(myprofile1.piggyback);
  myprofile2.piggyback_length = strlen(myprofile2.piggyback);

  printf("chan0_base64_isneeded: single OK: ");
  if (!chan0_base64_isneeded(&myprofile2)) {
    printf("PASSED: \n");
  } else {
    printf("FAIL\n");
    return(1);
  }

  printf("chan0_base64_isneeded: multi OK: ");
  if (!chan0_base64_isneeded(&myprofile1)) {
    printf("PASSED: \n");
  } else {
    printf("FAIL\n");
    return(1);
  }

  myprofile2.piggyback_length = strlen(myprofile2.piggyback) + 1;
  
  printf("chan0_base64_isneeded: single reqd: ");
  if (chan0_base64_isneeded(&myprofile2)) {
    printf("PASSED: \n");
  } else {
    printf("FAIL\n");
    return(1);
  }

  printf("chan0_base64_isneeded: multi reqd: ");
  if (chan0_base64_isneeded(&myprofile1)) {
    printf("PASSED: \n");
  } else {
    printf("FAIL\n");
    return(1);
  }
  

  return(0);
}
/*
 *
 */
void my_notify_lower(struct session * session, int code) {
  printf("notified lower: code=%d\n", code);
  return;
}

void my_notify_upper(struct session * session, long code1, int code2) {
  printf("notified upper: code1=%d code2=%d\n", (int)code1, (int)code2);
  return;
}

/*
 * main
 *
 * every self respecting test program has one.
 */
/*  typedef int (*intfp)(struct session *); */
typedef int (*intfp)();

int main () {
  int retcode;
  struct session mysession;
  intfp * fcurrent;

/*    intfp flist[] = { test_parse, NULL }; */
  intfp flist[] = { test_fmt, test_parse, test_base64_isneeded, NULL };

  memset(&mysession, 0, sizeof(struct session));

  mysession.malloc = malloc;
  mysession.free   = free;
  mysession.notify_lower   = my_notify_lower;
  mysession.notify_upper   = my_notify_upper;

  for (fcurrent = flist; NULL != *fcurrent; fcurrent++) {
    if ((retcode = (*fcurrent)(&mysession))) 
      exit(retcode);
  }

  exit(0);
}


