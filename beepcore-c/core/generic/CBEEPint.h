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
 * $Id: CBEEPint.h,v 1.2 2001/10/30 06:00:36 brucem Exp $
 *
 * CBEEPint.h
 *
 * Internal declarations for C BEEP library and Implementation
 *
 */

#ifndef __CBEEPINT_H__
#define __CBEEPINT_H__

#define __CBEEP_C__

#include "CBEEP.h"
#include "string.h"
#include "assert.h"
#include "stdio.h"

#define PRE(x) assert(x)
#define ASSERT(x) assert(x)
#define FAULT(s,i,t) fault(s,i,t,__FILE__,__LINE__)

  /* Define the guts of the session structure */
struct session {
  void * (*malloc)(size_t);
  void (*free)(void *);
  struct beep_iovec in_buffer;
  struct beep_iovec out_buffer;
  int status;
  char * status_text;
  long status_channel;
  long memory_limit;
  long memory_used;
  int frame_pre;  /* Extra memory allocation */
  int frame_post; /* Extra memory allocation */
  char IL;
  long close_status; /* if not -1, the message_number of the c0 message closing the session */
  long next_channel;
  long next_message;
  long tuning; /* The last arg to blu_tuning_reset */
  void (*notify_lower)(struct session *, int);
  void (*notify_upper)(struct session *, long, int);
  struct chan0_msg * remote_greeting; /* Until we dispose session */
  char * server_name; /* Until we dispose session */
  char * * offered_profiles; /* points to remote greeting */
  struct diagnostic * greeting_error; /* points to remote greeting */
  char * features; /* points to remote greeting */
  char * localize; /* points to remote greeting */
  void * session_info;
  struct channel * channel;
  char seq_buf[80];  /* Holds outgoing SEQ messages */
  char head_buf[80]; /* Holds outgoing frame headers */
  char read_buf[80]; /* Holds incoming frame headers */
  struct frame * read_frame; /* Partially read frame, NULL if reading header */
  unsigned long read_seq;    /* The sequence number I read for read_frame */
};


  /* Now a structure for remembering an ordered queue
     of message numbers, linked from the channel. 
     msg_more = '.' or '*' to indicate whether MSG
        has seen complete reply.
     rpy_more = ' ' or '*' or '.' to indicate that
        no reply has started, a reply has started,
        or a reply has finished, respectively.
     (NUL means reply has finished. 
      ANS more='.' means rpy_more='*').
     Algo: 
       When a frame arrives that's a MSG,
         If it's not on the received list,
           Add it.
         If it *is* on the received list,
           Make sure it's a *, or fault.
         If it's on the list, and msg_more is '.' and rpy_more is '.',
           Remove it from the list.
       When a frame arrives that's not a MSG
         If it's not on the sent list, fault.
         If it's on the sent list with rpy_more='.', fault.
         Set rpy_more to f->more (or '*' if f->msg_type='A')
         If rpy_more='.'&&msg_more='.', remove it.
       When a MSG is sent,
         If it's on the sent list with more='.', fault.
         If it's not on the sent list, add it.
         If it is on the sent list, msg_more <- frame->more
       When a non-MSG is sent,
         If it's not on the receive list, fault.
         If it's on the receive list with rpy_more='.', fault.
         Set rpy_more=f->more (except for f->msg_type='A')
         If rpy_more=='.' && msg_more=='.', remove it.
     Other Algo:
       if commit_frame==NULL&&in_frame==NULL&&out_frame==NULL&&sent==NULL&&rcvd==NULL then quiescent.
     */

struct uamn {
  struct uamn * next;
  long luamn;
  char msg_more;
  char rpy_more;
};

  /* Now the channel structure */
struct channel {
  struct channel * next;
  long channel_number;
  void * channel_info;
  int priority;
  char status; /* 'S' for starting, 'C' for closing, ' ' for normal, etc */
  int note_status; /* Last notify_upper status thingie. Look for it. */
  long c0_message_number;  /* The message number starting or closing this channel */
  struct frame * in_frame; /* first unobtained incoming frame */
  struct frame * out_frame; /* first uncommitted outgoing frame */
  struct frame * commit_frame; /* The frame currently being sent */
  struct uamn * sent; /* First on list is first to be answered */
  struct uamn * rcvd; /* First on list is first to be answered */
  long apparent_out_window; /* The biggest the sending window has been recently */
  char cur_out_msg_type;   /* The type of the message we're currently sending */
  long cur_out_msg_number; /* The number of the message we're currently sending */
  char prev_in_msg_type;   /* The type of the message we just received */
  long prev_in_msg_number; /* The number of the message we just received */
  char prev_in_msg_more;   /* Whether previous message was continued */

  unsigned long cur_in_seq;  /* The next sequence number expected on input */
  unsigned long max_in_seq;  /* The largest sequence number we've permitted in a sent SEQ */
  unsigned long cur_in_buf;  /* The number of bytes currently buffered. */
  unsigned long max_in_buf;  /* The number of bytes allowed to be buffered, default 4K */

  unsigned long cur_out_seq; /* The next sequence number to send on output */
  unsigned long max_out_seq; /* The largest seqno + 1 we may send right now */
};


extern void fault(struct session * session, int stat, char * test, 
		  char * file, int line);

#endif
