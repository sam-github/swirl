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
 * $Id: CBEEP.c,v 1.12 2002/04/27 15:23:18 huston Exp $
 *
 * CBEEP.c
 *
 * Internal declarations for C BEEP library and Implementation
 *
 */


#include "CBEEPint.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "channel_0.h"


  /* * * Now all the static functions that C is too stupid to make two passes to find * * */
  /* Could probably be arranged better */

  /* You found a fault. Remember it in the session, and call the right notification */
void fault(struct session * s, int stat, char * text,
	   char * file, int line) {
  s->status = stat;
  s->status_text = text;
  (*(s->notify_lower))(s, 1);
}

  /* Sequence number operations: the only real problem is comparisons.
     Hence, this function returns true if the first argument is less
     than the second, in wrapped math. All order comparisons on sequence
     numbers should use this code. */
#define HIBIT  0x80000000
#define MAXWIN 0x20000000
int ul_lt(unsigned long left, unsigned long right) {
    assert((left-right)!=HIBIT);
    return ((left-right)&HIBIT) != 0;
}

  /* Uses (*malloc) to allocate a string holding an error message */
static char * fmt_error(struct session * s, struct diagnostic * e) {
  /* Message is not mandatory (see example 2, RFC 3080, sec 2.4). */
  const char* emessage = e->message ? e->message : "";
  char * pattern;
  int size;
  int lang;
  char * result;

  /* SR The size calculation below assumes ecode is small (a bug?), so check. */
  PRE(e->code <= 999);

  lang = (e->lang != NULL && e->lang[0] != '\0');

  if (lang) {
    pattern = "<error code='%03d' xml:lang='%s'>%s</error>\r\n";
    size = strlen(pattern) + strlen(e->lang) + strlen(emessage);
  } else {
    pattern = "<error code='%03d'>%s</error>\r\n";
    size = strlen(pattern) + strlen(emessage);
  }

  result = (s->malloc)(size);
  if (result == NULL) return NULL;

  if (lang) {
    sprintf(result, pattern, e->code, e->lang, emessage);
  } else {
    sprintf(result, pattern, e->code, emessage);
  }
  return result;
}


  /* Uses (*malloc) to allocate a string holding the greeting */
static char * fmt_greeting(struct session * s, char * * profiles, 
    char * features, char * localize) {
  size_t size;
  char sbuf[300];
  int uri_inx;
  char * greeting;

  char * greet_head_1 = "<greeting";
  char * greet_head_2 = " features='%s'";
  char * greet_head_3 = " localize='%s'";
  char * greet_head_4 = ">\r\n";
  char * prof_head = "  <profile uri='%s'/>\r\n";
  char * greet_tail = "</greeting>\r\n";

  PRE(profiles != NULL);

  /* First figure out how big it will be, or a little bigger. */
  size = 1; /* for the trailing null */
  size += strlen(greet_head_1);
  if (features && *features) {
    size += strlen(greet_head_2) + strlen(features);
    PRE(strlen(features) < 200);
  }
  if (localize && *localize) {
    size += strlen(greet_head_3) + strlen(localize);
    PRE(strlen(localize) < 200);
  }
  size += strlen(greet_head_4);
  for (uri_inx = 0; profiles[uri_inx] != NULL; uri_inx += 1) {
    PRE(strlen(profiles[uri_inx]) < 200);
    size += strlen(prof_head) + strlen(profiles[uri_inx]);
  }
  size += strlen(greet_tail);

  /* Allocate that much space. */
  greeting = (s->malloc)(size);
  if (greeting == NULL) return NULL;

  /* And fill it in. */
  strcpy(greeting, greet_head_1);
  if (features && *features) {
    sprintf(sbuf, greet_head_2, features);
    strcat(greeting, sbuf);
  }
  if (localize && *localize) {
    sprintf(sbuf, greet_head_3, localize);
    strcat(greeting, sbuf);
  }
  strcat(greeting, greet_head_4);
  for (uri_inx = 0; profiles[uri_inx] != NULL; uri_inx += 1) {
    sprintf(sbuf, prof_head, profiles[uri_inx]);
    strcat(greeting, sbuf);
  }
  strcat(greeting, greet_tail);

  /* We're set */
  ASSERT(strlen(greeting) < size);
  return greeting;

}

  /* This function is called whenever messages are dequeued, in or
     out, to call the notify_upper with ANSWERED or QUIESCENT at
     the right time. Other calls to notify_upper are coded inline. */
static void call_notify_upper(struct session * s, struct channel * c) {
  int note_status;
  note_status = blu_channel_status(s, c->channel_number);
  if (c->note_status < note_status && 4 <= note_status) {
    if (c->note_status < 4) 
      (*(s->notify_upper))(s, c->channel_number, BLU_NOTEUP_ANSWERED);
    if (note_status == 5)
      (*(s->notify_upper))(s, c->channel_number, BLU_NOTEUP_QUIESCENT);
  }
  c->note_status = note_status; /* So we only notify once */
}

  /* Allocates a frame, does not charge for it. */
static struct frame * frame_create(struct session * s, int size) {
  struct frame * f;
  int allocated;
  allocated = size + 6 + sizeof(struct frame) + s->frame_pre + s->frame_post;
                         /* 6="END\r\n\0" */
  f = (*(s->malloc))(allocated);
  if (f != NULL) {
    f->session = s;
    f->next_in_message = NULL;
    f->next_on_channel = NULL;
    f->msg_type = '\0';
    f->channel_number = f->message_number = f->answer_number = -1;
    f->more = ' ';
    f->alloc = 0; /* Positive means it's locally allocated, negative means it's on a channel */
    f->size = size;
    f->payload = ((char *)f) + sizeof(struct frame) + s->frame_pre;
  } else {
    FAULT(s, 5, "In frame_create");
  }
  return f;
}


  /* This deallocates the frame and does any accounting necessary. 
     If c!=NULL, it saves us looking up the channel */
static void frame_destroy(struct frame * f, struct channel * c, struct session * s) {
  PRE(f != NULL);
  PRE(s != NULL);
  if (0 <= f->alloc) {
    s->memory_used -= f->alloc;
  } else {
    if (c == NULL || c->channel_number != f->channel_number) {
      /* Find the channel */
      c = s->channel;
      while (c && c->channel_number != f->channel_number) {
        c = c->next;
      }
    }
    if (c != NULL) { /* The channel is still open */
      c->cur_in_buf += f->alloc; /* f->alloc is negative for channels, so this actually subtracts */
    }
  }
  (*(s->free))(f);
}


  /* Link a forgotten channel into the session at the right place */
static void remember_channel(struct session * s, struct channel * c) {
  PRE(s != NULL);
  PRE(c != NULL);
  PRE(c->next == NULL);
  if (s->channel == NULL) {
    /* The easiest case - this is the only channel */
    s->channel = c;
  } else if (s->channel->priority <= c->priority) {
    /* Also easy. This channel has the highest priority */
    c->next = s->channel;
    s->channel = c;
  } else {
    /* We have to figure out where in the chain it goes. */
      /* It goes after `prev'. We insert it as the first element of its
         priority, behind all higher-priority elements and before all lower
         priority elements. */
    struct channel * prev;
    prev = s->channel;
    while (prev->next != NULL && c->priority < prev->next->priority) {
      prev = prev->next;
    }
    c->next = prev->next;
    prev->next = c;
  }
}

static void forget_channel(struct session * s, struct channel * c) {
  PRE(s != NULL);
  PRE(c != NULL);
  if (s->channel == c) {
    s->channel = c->next;
    c->next = NULL;
  } else {
    struct channel * prev;
    prev = s->channel;
    while (prev->next != NULL && prev->next != c) {
      prev = prev->next;
    }
    PRE(prev->next != NULL);
    prev->next = c->next;
    c->next = NULL;
  }
}

  /* remembers that this channel needs msg_no answered. Ensures it's linked to end of list. */
static struct uamn * add_rcvd_number(struct session * s, struct channel * c, long msg_no, char msg_more) {
  struct uamn * a;
  struct uamn * new_a;
  PRE(s != NULL);
  PRE(c != NULL);
  new_a = (*(s->malloc))(sizeof(*a));
  if (new_a == NULL) {
    FAULT(s, 5, "In add_rcvd_number");
    return NULL;
  } else {
    new_a->next = NULL;
    new_a->luamn = msg_no;
    new_a->msg_more = msg_more;
    new_a->rpy_more = ' ';
    a = c->rcvd;
    if (a == NULL) {
      c->rcvd = new_a;
    } else {
      while (a->next) a = a->next;
      a->next = new_a;
    }
    return new_a;
  }
}

  /* remembers that this channel sent msg_no. ensures it's linked to end of list */
     /* This can be unified with add_rcvd_number when we library the linked list stuff */
static struct uamn * add_sent_number(struct session * s, struct channel * c, long msg_no, char msg_more) {
  struct uamn * a;
  struct uamn * new_a;
  PRE(s != NULL);
  PRE(c != NULL);
  new_a = (*(s->malloc))(sizeof(*a));
  if (new_a == NULL) {
    FAULT(s, 5, "In add_sent_number");
    return NULL;
  } else {
    new_a->next = NULL;
    new_a->luamn = msg_no;
    new_a->msg_more = msg_more;
    new_a->rpy_more = ' ';
    a = c->sent;
    if (a == NULL) {
      c->sent = new_a;
    } else {
      while (a->next) a = a->next;
      a->next = new_a;
    }
    return new_a;
  }
}

  /* This returns a pointer to the received message number, or NULL if none */
     /* Again, this should be unified with find_sent_number */
struct uamn * find_rcvd_number(struct channel * c, long msg_no) {
  struct uamn * a;
  PRE(c != NULL);
  a = c->rcvd;
  while (a != NULL && a->luamn != msg_no)
    a = a->next;
  return a;
}

  /* This returns a pointer to the sent message number, or NULL if none */
     /* Again, this should be unified with find_rcvd_number */
struct uamn * find_sent_number(struct channel * c, long msg_no) {
  struct uamn * a;
  PRE(c != NULL);
  a = c->sent;
  while (a != NULL && a->luamn != msg_no)
    a = a->next;
  return a;
}

  /* This unlinks the sent message number, if it's there */
     /* Again, should be unified */
void remove_sent_number(struct session * s, struct channel * c, long msg_no) {
  struct uamn * a;
  struct uamn * pre;
  PRE(c != NULL);
  if (c->sent && c->sent->luamn == msg_no) {
    a = c->sent;
    c->sent = c->sent->next;
    (*(s->free))(a);
  } else {
    pre = c->sent;
    while (pre != NULL && pre->next != NULL && pre->next->luamn != msg_no)
      pre = pre->next;
    if (pre->next != NULL) {
      a = pre->next;
      pre->next = pre->next->next;
      (*(s->free))(a);
    }
  }
}

  /* This unlinks the received message number, if it's there */
     /* Again, should be unified */
void remove_rcvd_number(struct session * s, struct channel * c, long msg_no) {
  struct uamn * a;
  struct uamn * pre;
  PRE(c != NULL);
  if (c->rcvd && c->rcvd->luamn == msg_no) {
    a = c->rcvd;
    c->rcvd = c->rcvd->next;
    (*(s->free))(a);
  } else {
    pre = c->rcvd;
    while (pre != NULL && pre->next != NULL && pre->next->luamn != msg_no)
      pre = pre->next;
    if (pre->next != NULL) {
      a = pre->next;
      pre->next = pre->next->next;
      (*(s->free))(a);
    }
  }
}

void deal_with_sent_uamn(struct session * s, struct channel * c, struct frame * f) {
  struct uamn * a;
  if (f->msg_type == 'M') {
    /* Initiating a request */
    a = find_sent_number(c, f->message_number);
    if (a != NULL && a->msg_more == '.') {
      s->status_channel = f->channel_number;
      FAULT(s, 7, "Queued new message before previous answered");
    } else if (a != NULL) {
      a->msg_more = f->more;
    } else {
      a = add_sent_number(s, c, f->message_number, f->more);
    }
  } else {
    a = find_rcvd_number(c, f->message_number);
    if (a == NULL) {
      s->status_channel = f->channel_number;
      FAULT(s, 7, "Sending reply to unreceived message");
    } else if (a->rpy_more == '.') {
      s->status_channel = f->channel_number;
      FAULT(s, 7, "Sending reply to already-replied message");
    } else {
      if (f->msg_type != 'A') a->rpy_more = f->more;
      if (a->rpy_more == '.' && a->msg_more == '.')
        remove_rcvd_number(s, c, f->message_number);
    }
  }
}

void deal_with_rcvd_uamn(struct session * s, struct channel * c, struct frame * f) {
  struct uamn * a;
  if (f->msg_type == 'M') {
    a = find_rcvd_number(c, f->message_number);
    if (a == NULL) {
      add_rcvd_number(s, c, f->message_number, f->more);
    } else {
      if (a->msg_more == '.') {
        s->status_channel = f->channel_number;
        FAULT(s, 8, "Received same message number twice without reply");
      } else {
        a->msg_more = f->more;
        if (a->msg_more == '.' && a->rpy_more == '.') {
          remove_rcvd_number(s, c, f->message_number);
	}
      }
    }
  } else { /* Not an initial MSG */
    a = find_sent_number(c, f->message_number);
    if (a == NULL) {
      s->status_channel = f->channel_number;
      FAULT(s, 8, "Received reply for unsent message");
    } else if (a->rpy_more == '.') {
      s->status_channel = f->channel_number;
      FAULT(s, 8, "Received two replies for same message");
    } else {
      if (f->msg_type != 'A') a->rpy_more = f->more;
      if (a->msg_more == '.' && a->rpy_more == '.') {
        remove_sent_number(s, c, f->message_number);
      }
    }
  }
}

  /* Creates a channel, fills it in, links it to the session */
static struct channel * channel_create(struct session * sess, long channel_number) {
  struct channel * c;

  PRE(0 <= channel_number);

  c = (sess->malloc)(sizeof(struct channel));
  if (c == NULL) return NULL;
  c->next = NULL;
  c->channel_number = channel_number;;
  c->channel_info = NULL;
  c->priority = 100;
  c->status = ' '; c->c0_message_number = -1;
  c->note_status = 0;
  c->in_frame = NULL;
  c->out_frame = NULL;
  c->commit_frame = NULL;
  c->sent = NULL;
  c->rcvd = NULL;
/*c->apparent_out_window = 4096;*/
  c->cur_out_msg_type = ' ';
  c->cur_out_msg_number = -1;
  c->prev_in_msg_type = ' ';
  c->prev_in_msg_number = -1;
  c->prev_in_msg_more = '.';
  c->cur_in_seq = 0;
  c->max_in_seq = 4096;
  c->cur_in_buf = 0;
  c->max_in_buf = 4096;
  c->cur_out_seq = 0;
  c->max_out_seq = 4096;
  remember_channel(sess, c);

/* For testing, knock this down */
/*
c->cur_out_seq = 4000;
*/

  return c;
}

  /* Destroys a channel, reaping everything, after unlinking it from the session */
static void channel_destroy(struct session * sess, struct channel * chan) { // should take a **chan, and NULL it
  struct frame * f;
  struct uamn * a;

  PRE(sess != NULL);
  PRE(chan != NULL);

  forget_channel(sess, chan);
  if (chan->commit_frame) frame_destroy(chan->commit_frame, chan, sess);
  while (chan->in_frame) {
    f = chan->in_frame;
    chan->in_frame = f->next_on_channel;
    frame_destroy(f, chan, sess);
  }
  while (chan->out_frame) {
    f = chan->out_frame;
    chan->out_frame = f->next_on_channel;
    frame_destroy(f, chan, sess);
  }
  while (chan->sent) {
    a = chan->sent;
    chan->sent = a->next;
    (*(sess->free))(a);
  }
  while (chan->rcvd) {
    a = chan->rcvd;
    chan->rcvd = a->next;
    (*(sess->free))(a);
  }
  (*(sess->free))(chan);

}

  /* Clean up the commited frame pointer if necessary. Ignore NUL unless doNUL. */
     /* We do this in two passes because NULs are always size==0, but we can't
	dispose the NUL frame until we build the header to send. */
     /* @$@$ I'm not sure this works right for 0-length non-NUL frames.
        that probably indicates that the whole approach is wrong. */
static void clean_commit(struct session * s, struct channel * c, int doNUL) {

  /* NUL and non-NUL is different because NUL is allowed to be zero-length */

  PRE(s != NULL);
  if (c == NULL) return;
  if (c->commit_frame && c->commit_frame->size <= 0) {
    /* Let's clean it up. */
    if (c->commit_frame->msg_type == 'M' && !doNUL) {
      if (c->commit_frame->more == '.') {
	/* We finished sending the entire message */
	c->cur_out_msg_type = ' ';
	c->cur_out_msg_number = -1;
      }
      /* Now clean up the frame itself */
      frame_destroy(c->commit_frame, c, s);
      c->commit_frame = NULL;
    } else if ((c->commit_frame->msg_type != 'N' && !doNUL)
            || (c->commit_frame->msg_type == 'N' &&  doNUL)) {
      /* We're answering */
      if (c->commit_frame->more == '.' && c->commit_frame->msg_type != 'A') {
	/* We finished sending the entire response */
	c->cur_out_msg_type = ' ';
	c->cur_out_msg_number = -1;
      }
      /* Now clean up the frame itself */
      frame_destroy(c->commit_frame, c, s);
      c->commit_frame = NULL;
    }
    /* Here, we cleaned up a frame. If the queue is empty, signal that. 
     * This isn't especially useful, state-machine-wise. */
    if (c->out_frame == NULL) {
      (*(s->notify_upper))(s, c->channel_number, BLU_NOTEUP_QEMPTY);
    }
    /* Then, if the status has gone higher, let them know. */
    call_notify_upper(s, c);
  }
}

  /* For channel 'c', this picks the next frame to send. 
     Somehow, this has to keep track of partial messages and only queue
     frames from said partial messages. */
static void pick_frame_to_commit(struct session * s, struct channel * c) {
  struct frame * p;
  struct frame * f;
  PRE(s != NULL);
  if (c == NULL) return;
  if (c->commit_frame != NULL) return; /* Already have one, thanks. */
  if (c->out_frame == NULL) return;    /* Sorry, gave at the office. */
  /* OK, now... If we're already in the middle of sending something,
     pick the next frame available from that something. Otherwise,
     pick the next frame on the queue. Caveat Empor! @$@$ The sender
     should not queue the NUL before all ANS frames are completely queued. */
  if (c->cur_out_msg_number == -1) {
    /* Nothing in progress */
    c->commit_frame = c->out_frame;
    c->out_frame = c->out_frame->next_on_channel;
    c->commit_frame->next_on_channel = NULL;
    c->cur_out_msg_type = c->commit_frame->msg_type;
    c->cur_out_msg_number = c->commit_frame->message_number;
  } else {
    /* Find a matching frame, if any */
#ifdef	MTR
if (c -> out_frame) {
    fprintf (stderr, "channel %ld looking for type %c msgno %ld\n",
	     c -> channel_number, c -> cur_out_msg_type,
	     c -> cur_out_msg_number);
    fflush(stderr);
}
#endif 
    for (f = c -> out_frame; f; f = f -> next_on_channel)
        if ((c -> cur_out_msg_number == f -> message_number)
	        && ((c -> cur_out_msg_type == f -> msg_type)
		        || ((c -> cur_out_msg_type == 'A')
			        && (f -> msg_type == 'N'))))
	    break;
    /* Now, either f points to a matching frame, or it ran off the list. */
    if (f == NULL) {
      /* No matching frame waiting. Do nothing */
      return;
    } else {
      /* Found a match. Now we have to unlink it. */
      if (c->out_frame == f) {
	c->out_frame = f->next_on_channel;
      } else {
	p = c->out_frame;
	while (p->next_on_channel != f) p = p->next_on_channel;
	p->next_on_channel = f->next_on_channel;
      }
      c->commit_frame = f;
    }
  }
  deal_with_sent_uamn(s, c, c->commit_frame); /* Make sure we handle it */
}


  /* This formats the header of frame 'f' into the head_buf in 's',
     except with a maximum size of 'size' */
static void fmt_head(struct session * s, struct channel * c, 
                     struct frame * f, long size) {
  char * h1;
  char more;

  switch (f->msg_type) {
    case 'M' : h1 = "MSG"; break;
    case 'R' : h1 = "RPY"; break;
    case 'A' : h1 = "ANS"; break;
    case 'E' : h1 = "ERR"; break;
    case 'N' : h1 = "NUL"; break;
    default : h1 = "???"; break;
  }

  if (size == f->size) {
    more = f->more;
  } else {
    more = '*';
  }

  sprintf(s->head_buf, "%s %ld %ld %c %lu %ld",
    h1, f->channel_number, f->message_number, more,
    c->cur_out_seq, size);
  if (f->msg_type == 'A') {
    sprintf(s->head_buf + strlen(s->head_buf), " %ld", f->answer_number);
  }
  strcat(s->head_buf, "\r\n");
  ASSERT(strlen(s->head_buf) < sizeof(s->head_buf)-1);
}


  /* This finds the best channel to send a SEQ on by looking for the 
     one with the biggest outstanding window. 
     Notes about the SEQ stuff in the channel structure:
     max_in_buf is the window size the user has set, default 4K.
       It's the maximum we're willing to buffer in the library.
     cur_in_buf is the amount that's actually buffered. It goes up
       as things arrive and goes down as the profile consumes things.
     max_in_seq is from the most recent SEQ sent:
       the ackno is added to the window to get this value.
     cur_in_seq is the next sequence number expected.
     Hence,
       max_in_buf-cur_in_buf is the space left in the buffer.
       max_in_seq-cur_in_seq is the space left in the stream.
       buffer space - stream space is the value to find the biggest of.
     */
  /* BUGGY! THIS ASSUMES NO 2^32-1 WRAPPING! Use "long long" here? */
/*
SR - this always sends a SEQ (which can trigger SWS), and never sends multiple
SEQ (bad if you actually have lots of channels, it would seem to me).

Also, I think the SEQ buffer should be built on reception of a frame:

  each time a frame is received, a SEQ frame should be sent whenever the window
  size that will be sent is at least one half of the buffer space available to
  this channel; and,

    - RFC3081, 3.2.4

That section has other advice which is not being followed here.
*/
static void find_best_seq_to_send(struct session * s) {
  struct channel * best; /* Best */
  struct channel * test; /* test */
  long b_size;
  long t_size;

  if (s->channel == NULL) {
    s->seq_buf[0] = '\0';
    return;
  }
  test = s->channel;
  best = s->channel; /* Initially, the best is the first one we tested */
  while (test != NULL) {
    b_size = (best->max_in_buf - best->cur_in_buf) - 
             (best->max_in_seq - best->cur_in_seq);
    t_size = (test->max_in_buf - test->cur_in_buf) - 
             (test->max_in_seq - test->cur_in_seq);
    if (ul_lt(b_size, t_size)) {
      best = test;
    }
    test = test->next;
    if (best->status == 'C' || best->status == 'S' ||
        (s->tuning != -1 && s->tuning != best->channel_number)) {
        best = test;
    }
  }
  if (best != NULL && s->tuning == -1) {
    b_size = (best->max_in_buf - best->cur_in_buf) - 
             (best->max_in_seq - best->cur_in_seq);
    if (0 < b_size) {
      /* Build the SEQ message */
      sprintf(s->seq_buf, "SEQ %ld %lu %lu\r\n", 
        best->channel_number, best->cur_in_seq, best->max_in_buf - best->cur_in_buf);
      ASSERT(strlen(s->seq_buf) < sizeof(s->seq_buf)-1);
      best->max_in_seq = best->cur_in_seq + best->max_in_buf - best->cur_in_buf;
    }
  }
}

  /* Call FAULT and return false if not valid to be received now. Else return true. 
     Maintains state in channel to check this. */
static int check_valid_in(struct session * s, struct channel * c, struct frame * f) {
  struct uamn * a;
  /* Check, if MSG, that it's not already outstanding */
  if (f->msg_type == 'M') {
    a = find_rcvd_number(c, f->message_number);
    if (a != NULL && a->msg_more == '.') {
      s->status_channel = c->channel_number;
      FAULT(s, 8, "Received reused message number before answer complete");
      return 0;
    }
  }
  /* Check, if !MSG, that the MSG is outstanding */
  if (f->msg_type != 'M' && NULL == find_sent_number(c, f->message_number)) {
    s->status_channel = c->channel_number;
    FAULT(s, 8, "Received reply to unsent or completely-answered message");
    return 0;
  }
  /* Check that the TLA matches the previous with the same msgno
          E.g., don't interleave RPY and ANS */
  if (c->prev_in_msg_more == '*' && f->msg_type != c->prev_in_msg_type && 
      f->msg_type != 'N') {
    s->status_channel = c->channel_number;
    FAULT(s, 8, "Received message of mixed type");
    return 0;
  }
  /* Check that you're not interleaving message numbers */
  if (c->prev_in_msg_more == '*' && f->message_number != c->prev_in_msg_number) {
    s->status_channel = c->channel_number;
    FAULT(s, 8, "Received message of mixed number");
    return 0;
  }
  if (f->msg_type == 'N') {
    /* No NUL if previous (if any) wasn't ANS */
    if (c->prev_in_msg_more != '.' && c->prev_in_msg_type != 'A') {
      s->status_channel = c->channel_number;
      FAULT(s, 8, "Received NUL after non-ANS");
      return 0;
    }
    /* NUL continued or not empty */
    if (f->more != '.' || f->size != 0) {
      s->status_channel = c->channel_number;
      FAULT(s, 8, "Received NUL either continued or not empty");
      return 0;
    }
  }
  /* prev->more=='*' && this->msgno != prev->msgno */
  if (c->prev_in_msg_more == '*' && f->message_number != c->prev_in_msg_number) {
    s->status_channel = c->channel_number;
    FAULT(s, 8, "Continued message has different message number");
    return 0;
  }
  c->prev_in_msg_more = f->more;
  c->prev_in_msg_type = f->msg_type;
  c->prev_in_msg_number = f->message_number;
  if (f->msg_type == 'A')
    c->prev_in_msg_more = '*';
  return 1;
}

  /* This takes a completed frame in s->read_frame and does the right thing with it. */
static void place_frame(struct session * s) {
  struct frame * f;
  struct channel * c;

  if (0 != strncmp(s->read_frame->payload + s->read_frame->size, "END\r\n", 5)) {
    FAULT(s, 8, "END\\r\\n not found");
    (*(s->free))(s->read_frame);
    s->read_frame = NULL;
    return;
  }
  s->read_frame->payload[s->read_frame->size] = '\0'; /* Chuck the END\r\n for convenience */
  c = s->channel;
  while (c && c->channel_number != s->read_frame->channel_number)
    c = c->next;
  c->cur_in_seq += s->read_frame->size;
  c->cur_in_buf += s->read_frame->size;

  if (!check_valid_in(s, c, s->read_frame)) {
    frame_destroy(s->read_frame, c, s);
    s->read_frame = NULL;
  }

  /* Add it to the channel */
  if (c->in_frame == NULL) {
    c->in_frame = s->read_frame;
  } else {
    f = c->in_frame;
    while (f->next_on_channel != NULL) 
      f = f->next_on_channel;
    f->next_on_channel = s->read_frame;
  }
  s->read_frame->next_on_channel = NULL;
  f = s->read_frame; s->read_frame = NULL;
  deal_with_rcvd_uamn(s, c, f);

  /* Okey. Let's do that good old notify_upper thing. */
  (*(s->notify_upper))(s,f->channel_number,BLU_NOTEUP_FRAME);
  if (f->more == '.') {
    (*(s->notify_upper))(s,f->channel_number,BLU_NOTEUP_MESSAGE);
  }
  if (c->max_in_buf-c->cur_in_buf == 0) {
    (*(s->notify_upper))(s,f->channel_number,BLU_NOTEUP_WINDOWFULL);
  }

  /* Small frob to handle channel 0 messages proactively,
     because we're lazy and don't do partial parsing.  */
  if (f->channel_number == 0 && c->max_in_buf-c->cur_in_buf == 0) {
    f = c->in_frame;
    while (f != NULL) {
      if (f->more == '.') break;
      f = f->next_on_channel;
    }
    if (f == NULL) {
      /* Didn't find a complete frame on channel 0 */
      s->status_channel = 0;
      FAULT(s, 8, "Channel 0 message much too large to buffer");
    }
  }

}

  /* This parses a SEQ header, updates *s, and moves the rest of the record
     to the start of the read_buf */
static void parse_seq(struct session * s) {
  char * p;
  long channel;
  unsigned long ackno;
  long window;
  unsigned long new_max_out_seq;
  int remains;

  ASSERT(s != NULL);
  p = s->read_buf;
  if (p[0] != 'S' || p[1] != 'E' || p[2] != 'Q' || p[3] != ' ') {
    FAULT(s, 8, "SEQ spelled wrong");
  } else {
    p += 4;
    channel = 0;
    while ('0' <= *p && *p <= '9') {
      channel = 10 * channel + *p - '0';
      p += 1;
    }
    if (*p != ' ') {
      FAULT(s, 8, "Missing space after channel in SEQ");
    } else {
      p += 1;
      ackno = 0;
      while ('0' <= *p && *p <= '9') {
        ackno = 10 * ackno + *p - '0';
        p += 1;
      }
      if (*p != ' ') {
        FAULT(s, 8, "Missing space after ackno in SEQ");
      } else {
        p += 1;
        window = 0;
        while ('0' <= *p && *p <= '9') {
          window = 10 * window + *p - '0';
          p += 1;
        }
        if (*p != '\r' || *(p+1) != '\n') {
          FAULT(s, 8, "Missing CRLF on SEQ");
        } else {
          struct channel * c;
          c = s->channel;
          while (c && c->channel_number != channel) c = c->next;
          if (c == NULL) {
            FAULT(s, 8, "Channel in SEQ not open");
          } else {
            new_max_out_seq = ackno + window; /* @$@$ Needs proper wrapping */
            if (ul_lt(new_max_out_seq,  c->max_out_seq)) {
              FAULT(s, 8, "Window moved wrong direction in SEQ");
            } else {
              c->max_out_seq = new_max_out_seq;
              /* Good. Now, let's fix the read_buf, in case we read past the CRLF */
              /* Above, p[1]=='\n', so move p[2] down to read_buf[0] and fix everything up. */
              p += 2; 
              remains = ((char*)(s->in_buffer.iovec[0].iov_base)) - p;
              memmove(s->read_buf, p, remains); /* Must handle overlaps properly */
              /* OK. Now the rest of the read data is at the start of read_buf */
              s->in_buffer.iovec[0].iov_len = sizeof(s->read_buf) - remains - 2;
              s->in_buffer.iovec[0].iov_base = s->read_buf + remains;
	      /* send any queued frames */
              (*(s->notify_lower))(s, 3);
              /* And stay in header mode here. */
            }
          }
        }
      }
    }
  }
}

  /* This parses a non-SEQ header, updates *s, allocates a frame,
     links it into s->read_frame, and points the iov_base at the
     frame payload, then switches to payload mode. 
     Careful of 0-length frames. Also updates windows. */
static void parse_head(struct session * s) {
  int broken;
  char * p;
  char msg_type = ' ';      /* Initializations purely to shut up warnings */
  long channel_number = -1;
  long message_number = -1;
  long answer_number = -1;
  char more = ' ';
  long size = 0;
  unsigned long seq = 0;
  struct frame * f;
  int remains;
  struct channel * c;

  ASSERT(s != NULL);
  broken = 0;
  p = s->read_buf;
  if (0!=strncmp(p, "MSG ", 4) && 
      0!=strncmp(p, "RPY ", 4) &&
      0!=strncmp(p, "ERR ", 4) &&
      0!=strncmp(p, "ANS ", 4) &&
      0!=strncmp(p, "NUL ", 4)) {
    FAULT(s, 8, "Invalid message type");
    broken = 1;
  } else {
    msg_type = p[0];
  }
  if (!broken) {
    p += 4;
    channel_number = 0;
    while ('0' <= *p && *p <= '9') 
      channel_number = 10 * channel_number + *(p++) - '0';
    if (*p != ' ') {
      FAULT(s, 8, "Missing space after channel number");
      broken = 1;
    }
  }
  if (!broken) {
    p += 1; /* skip the space */
    message_number = 0;
    while ('0' <= *p && *p <= '9') 
      message_number = 10 * message_number + *(p++) - '0';
    if (*p != ' ') {
      FAULT(s, 8, "Missing space after message number");
      broken = 1;
    }
  }
  if (!broken) {
    p += 1; /* skip the space */
    if (*p == '*' || *p == '.') {
      more = *p;
      p += 1;
      if (*p != ' ') {
        FAULT(s, 8, "No space after more character");
        broken = 1;
      }
    } else {
      FAULT(s, 8, "Invalid more character");
      broken = 1;
    }
  }
  if (!broken) {
    p += 1; /* skip the space again */
    seq = 0;
    while ('0' <= *p && *p <= '9') 
      seq = 10 * seq + *(p++) - '0';
    if (*p != ' ') {
      FAULT(s, 8, "Missing space after sequence number");
      broken = 1;
    }
  }
  if (!broken) {
    p += 1; /* skip the space */
    size = 0;
    while ('0' <= *p && *p <= '9') 
      size = 10 * size + *(p++) - '0';
  }
  if (!broken && msg_type != 'A') {
    if (p[0] != '\r' || p[1] != '\n') {
      FAULT(s, 8, "Missing CRLF after size");
      broken = 1;
    }
  } else if (!broken && msg_type == 'A') {
    if (*p != ' ') {
      FAULT(s, 8, "Missing space after size");
      broken = 1;
    }
  }
  if (!broken && msg_type == 'A') {
    p += 1; /* skip the space */
    answer_number = 0;
    while ('0' <= *p && *p <= '9') 
      answer_number = 10 * answer_number + *(p++) - '0';
    if (p[0] != '\r' || p[1] != '\n') {
      FAULT(s, 8, "Missing CRLF after answer number");
      broken = 1;
    }
  }
  if (broken) return;
  /* Wheee! Parsing is done! */

  /* Let's check for errors before we allocate a frame */
  c = s->channel;
  while (c && c->channel_number != channel_number)
    c = c->next;
  if (c == NULL) {
    FAULT(s, 8, "Frame received for closed channel");
    return;
  }
  if (seq != c->cur_in_seq) {
    FAULT(s, 8, "Frame received with unexpected sequence number");
    return;
  }
  if (ul_lt(c->max_in_seq, seq + size)) {
    FAULT(s, 8, "Frame too large for window");
    return;
  }

  /* Now we allocate a frame, move the start of the payload into it, 
     fill out the header, and fill up the payload the rest of the way. */
  f = frame_create(s, size);
  if (f == NULL) {
    FAULT(s, 5, "Out of Memory");
  } else {
    f->msg_type = msg_type;
    f->channel_number = channel_number;
    f->message_number = message_number;
    f->answer_number = answer_number;
    f->more = more;
    f->alloc = - size; /* Negative means it counts against the channel's window */
    s->read_seq = seq; /* Just for error checking */
    p += 2;  /* Skip CRLF */
    remains = ((char*)(s->in_buffer.iovec[0].iov_base)) - p;
    if (remains < size + 5) {
      /* Frame payload + END\r\n did not fit entirely in read_buf */
      memcpy(f->payload, p, remains);
      s->in_buffer.iovec[0].iov_base = f->payload + remains;
      ((char*)(s->in_buffer.iovec[0].iov_base))[0] = '\0';
      s->in_buffer.iovec[0].iov_len = size - remains + 5; /* for END\r\n */
      s->read_frame = f;
      s->read_buf[0] = '\0'; /* Just to make debugging easier. */
    } else {
      /* Entire frame fit within read_buf. Move just that part. */
      memcpy(f->payload, p, size + 5);
      remains -= (size + 5);
      p += size + 5;
      s->read_frame = f;
      place_frame(s);
      /* Now move anything that comes after back into the read_buf */
      ASSERT(0 <= remains);
      ASSERT(remains < sizeof(s->read_buf));
      memcpy(s->read_buf, p, remains);
      /* OK. Now the rest of the read data is at the start of read_buf */
      s->in_buffer.iovec[0].iov_len = sizeof(s->read_buf) - remains - 2;
      s->in_buffer.iovec[0].iov_base = s->read_buf + remains;
    }
  }
}

static void bump_next_message(struct session * s) {
  /* Change the "next_message" entry. Note the math is
     just easier to make visually distinctive message numbers. */
  /* s->next_message = (s->next_message * 17) % 1000000000; */
  s->next_message = (s->next_message + 1) % 1000000000;
  /* if (s->next_message < 10) s->next_message += 10; */
  /* Besides being lame, not using chan zero msg numbers starting from 1 surprises
   * other (admittedly non-conformant) toolkits.
   */
}

  /* * * +=+=+=+ Now the real work +=+=+=+ * * */

struct session * bll_session_create(
  void *(*malloc)(size_t),
  void (*free)(void*),
  void (*notify_lower)(struct session *,int),
  void (*notify_upper)(struct session *,long,int),
  char IL,
  char * features,
  char * localize,
  char * * profiles,
  struct diagnostic * error,
  void *user_data) {

  struct session * newsession;
  struct channel * chan0;
  struct uamn * a;
  struct frame * greeting;
  char * mime = "Content-Type: application/beep+xml\r\n\r\n";
  char * payload;

  ASSERT(sizeof(long) == 4); /* Sorry about that. */

  newsession = (*malloc)(sizeof(struct session));
  if (newsession == NULL) return NULL;
  memset(newsession, 0, sizeof(*newsession));
  newsession->out_buffer.vec_count = 0;
  newsession->in_buffer.vec_count = 1;
  newsession->in_buffer.iovec[0].iov_len = sizeof(newsession->read_buf)-2;
  newsession->in_buffer.iovec[0].iov_base = newsession->read_buf;
  newsession->status = 0;
  newsession->status_text = NULL;
  newsession->status_channel = -1;
  newsession->memory_limit = -1; /* No limit */
  newsession->memory_used = 0;
  if (IL == 'I') 
    newsession->next_channel = 1;
  else
    newsession->next_channel = 2;
  newsession->next_message = 0;
  newsession->tuning = -1;
  newsession->malloc = malloc;
  newsession->free = free;
  newsession->frame_pre = 0;
  newsession->frame_post = 0;
  newsession->IL = IL;
  newsession->close_status = -1;
  newsession->notify_lower = notify_lower;
  newsession->notify_upper = notify_upper;
  newsession->remote_greeting = NULL;
  newsession->server_name = NULL;
  newsession->offered_profiles = NULL;
  newsession->greeting_error = NULL;
  newsession->features = NULL;
  newsession->localize = NULL;
  newsession->session_info = user_data;
  newsession->channel = NULL;
  newsession->seq_buf[0] = '\0';
  newsession->head_buf[0] = '\0';
  newsession->read_frame = NULL;

  PRE(malloc != NULL);
  PRE(free != NULL);
  PRE(IL=='I' || IL=='L');
  PRE(notify_lower != NULL);
  PRE(notify_upper != NULL);
  PRE(profiles != NULL || error != NULL);

  /* Then create channel zero */
  chan0 = channel_create(newsession, 0);
  if (chan0 == NULL) {
    (*free)(newsession);
    return NULL;
  }
  chan0->priority = 101;

  /* Build the payload for the greeting message */
  if (error != NULL) {
    payload = fmt_error(newsession, error);
  } else {
    payload = fmt_greeting(newsession, profiles, features, localize);
  }
  if (payload == NULL) {
    (*free)(chan0);
    (*free)(newsession);
    return NULL;
  }
  if (NULL == (a = add_rcvd_number(newsession, chan0, 0, '.'))) {
    (*free)(payload);
    (*free)(chan0);
    (*free)(newsession);
    return NULL;
  }
  if (NULL == add_sent_number(newsession, chan0, 0, '.')) {
    (*free)(a);
    (*free)(payload);
    (*free)(chan0);
    (*free)(newsession);
    return NULL;
  }

  /* Then allocate the greeting frame */
  greeting = frame_create(newsession, strlen(payload)+strlen(mime)+1);
          /* Room for the trailing \0 */
  if (greeting == NULL) {
    (*free)(payload);
    (*free)(chan0);
    (*free)(newsession);
    return NULL;
  }
  greeting->alloc = strlen(payload) + strlen(mime) + 1;

  /* Fill in the headers of the frame */
  if (error != NULL) {
    greeting->msg_type = 'E';
  } else {
    greeting->msg_type = 'R';
  }
  greeting->channel_number = 0;
  greeting->message_number = 0;
  greeting->more = '.';

  /* Fill in the payload with the offered profiles or error */
  strcpy(greeting->payload, mime);
  strcat(greeting->payload, payload);
  (*free)(payload);
  ASSERT(((long)strlen(greeting->payload)) < greeting->size);
  greeting->size = strlen(greeting->payload);
  newsession->memory_used = greeting->alloc;

  /* Commit the greeting so we're sure it goes first */
  chan0->out_frame = greeting;
  pick_frame_to_commit(newsession, chan0);
  (void) bll_out_buffer(newsession);

  /* Now bump window size so remote can send us a fully-filled <start> */
  if (profiles != NULL) {
    int count; int uri_len;
    for (uri_len = count = 0; profiles[count] != NULL; count += 1) 
      uri_len += strlen(profiles[count]);
    blu_local_window_set(newsession, 0, (5100*count + uri_len));
    /* Enough to hold URIs, <start>/<profile> stuff, and 4K piggybacks */
  }    

  /* Return success. */
  return newsession;
}

int bll_status(struct session * s) {
  PRE(s != NULL);
  return s->status;
}
long bll_status_channel(struct session * s) {
  PRE(s != NULL);
  return s->status_channel;
}
char * bll_status_text(struct session * s) {
  PRE(s != NULL);
  return s->status_text;
}

long bll_memory_limit(struct session * s, long new_limit) {
  long old_limit;
  PRE(s != NULL);
  PRE(-1 <= new_limit);
  if (MAXWIN <= new_limit)
    new_limit = MAXWIN - 1;
  old_limit = s->memory_limit;
  s->memory_limit = new_limit;
  return old_limit;
}


  /* OK. This has to first queue the frame to the channel. */
void blu_frame_send(struct frame * f) {
  struct session * s;
  struct channel * c;
  struct frame * prev;

  PRE(f != NULL);
  PRE(f->next_in_message == NULL);
  PRE(f->next_on_channel == NULL);
  PRE(0 <= f->channel_number); /* Should be 0 < f->channel_number, but we use this internally @$@$ */
  PRE(f->more == '.' || f->more == '*');
  PRE(-1 != f->message_number || f->more == '.');
  PRE('A' != f->msg_type || -1 != f->message_number || f->more == '.');

  s = f->session;
  ASSERT(s != NULL);
  c = s->channel;
  ASSERT(c != NULL);
  /* @$@$ Add all the other checks here. */
  /* @$@$ Maybe fix next_in_frame to be next_on_channel */

  if (f->message_number == -1) {
    /* Assume "PRE" above caught misuse of this */
    bump_next_message(s);
    f->message_number = s->next_message;
  }
  if (f->msg_type == 'A' && f->answer_number == -1) {
    /* Assume "PRE" above caught misuse of this */
    bump_next_message(s);
    f->answer_number = s->next_message;
  }

  if (0 < s->status) {
    /* Fault on session. Just toss the frame. */
    frame_destroy(f, NULL, s);
    return;
  }

  f->next_on_channel = NULL;

  while (c && c->channel_number != f->channel_number) {
    c = c->next;
  } 
  if (c == NULL) {
    s->status_channel = f->channel_number;
    FAULT(s, 7, "blu_frame_send on closed channel");
    frame_destroy(f, NULL, s);
    return;
  }
  if (c->status == 'S') {
    s->status_channel = f->channel_number;
    FAULT(s, 7, "blu_frame_send on half-started channel");
    frame_destroy(f, NULL, s);
    return;
  }
  if (c->status == 'C' && f->msg_type == 'M') {
    s->status_channel = f->channel_number;
    FAULT(s, 7, "blu_frame_send sending MSG on closing channel");
    frame_destroy(f, NULL, s);
    return;
  }
  if (c->out_frame == NULL) {
    c->out_frame = f;
  } else {
    prev = c->out_frame;
    while (prev->next_on_channel != NULL)
      prev = prev->next_on_channel;
    prev->next_on_channel = f;
  }

  (void) bll_out_buffer(s); /* Move it to sending */
}

struct beep_iovec * bll_out_buffer(struct session * s) {
  struct channel * c;
  struct channel * b;

  PRE(s != NULL);

  if (0 < s->status) 
    return NULL; /* Fault on session. Stop sending. */

  if (s->out_buffer.vec_count != 0)
    return &(s->out_buffer);

  /* Buffer is empty. Let's fill it up. */
  /* First, see if there's a SEQ to send */
  s->seq_buf[0] = '\0';
  find_best_seq_to_send(s);
  if (s->seq_buf[0] != '\0') {
    s->out_buffer.iovec[0].iov_base = s->seq_buf;
    s->out_buffer.iovec[0].iov_len  = strlen(s->seq_buf);
    s->out_buffer.vec_count = 1;
  }

  /* Now, for ease of degubbing, clear out other buffers */
  s->head_buf[0] = '\0';

  /* Now let's find any channels with a frame that's finished sending
     and clean it up. Then pick the next frame to commit. */
  c = s->channel;
  while (c) {
    clean_commit(s, c, 0); /* Don't clean up NUL yet */
    pick_frame_to_commit(s, c);
#ifdef	MTR
if (c -> commit_frame) {
    fprintf (stderr, "channel %ld type %c msgno %ld\n", c -> channel_number,
	     c -> commit_frame -> msg_type,
	     c -> commit_frame -> message_number);
    fflush (stderr);
}
#endif 
    c = c->next;
  }

  /* Now find the channel with the biggest open outgoing window
     that has something committed. Alternately, pick the first
     channel, which will have the highest priority. */
/*  
  c = s->channel; b = c;
  while (c) {
    if (b->commit_frame == NULL || (
	  c->commit_frame != NULL && 
          ul_lt((b->max_out_seq - b->cur_out_seq),
            (c->max_out_seq - c->cur_out_seq)))) {
      b = c;
    }
    c = c->next;
  }
*/
  /* Alternate priority-based implementation: */
  c = s->channel; b = NULL;
  while (c) {
    if (c->commit_frame != NULL && ul_lt(c->cur_out_seq, c->max_out_seq) &&
        (s->tuning == -1 || c->channel_number == s->tuning)) {
      b = c;
      break;
    }
    c = c->next;
  }
/*
SR

Both schemes above ignore RFC3081, 3.1.4:

  frames for different channels with traffic ready to send should be sent in a
  round-robin fashion

Though it is true that chan0 needs special attention.
*/

  /* Now b points to the best channel to send a frame from. */
  if (b && b->commit_frame && ul_lt(b->cur_out_seq, b->max_out_seq)) {
    long size; int v;
    /* @$@$ Here is where you have to patch in pre- and post-frame processing */
    size = b->max_out_seq - b->cur_out_seq;
    /* RFC3081, 3.1.4:
     * large messages should be segmented into frames no larger than two-thirds of
     * TCP's negotiated maximum segment size
     *
     * TODO - doing it this way causes us to do a round-trip through swirl, and
     * write(2) for every frame. What should be done is the iovec should be used
     * to put window size worth of data into a series of 2/3 MSS frames, which
     * then all get sent in a single writev().
     */
    if(s->max_frame_size && size > s->max_frame_size) {
      size = s->max_frame_size;
    }
    if (b->commit_frame->size < size) size = b->commit_frame->size;
    fmt_head(s, b, b->commit_frame, size);
    v = s->out_buffer.vec_count;
    s->out_buffer.iovec[v].iov_base = s->head_buf;
    s->out_buffer.iovec[v].iov_len = strlen(s->head_buf);
    v += 1;
    if (0 < size) {
      s->out_buffer.iovec[v].iov_base = b->commit_frame->payload;
      s->out_buffer.iovec[v].iov_len = size;
      v += 1;
    }
    s->out_buffer.iovec[v].iov_base = "END\r\n";
    s->out_buffer.iovec[v].iov_len = 5;
    v += 1;
    s->out_buffer.vec_count = v;
    b->commit_frame->size -= size;
    b->commit_frame->payload += size;
    b->cur_out_seq += size;
  }

  /* If that just sent a NUL, then clean it up. */
  c = s->channel;
  while (c) {
    clean_commit(s, c, 1); /* Now deal with NUL frames */
    c = c->next;
  }
  if (0 < s->out_buffer.vec_count) {
    (*(s->notify_lower))(s, 3);
  }
  return &(s->out_buffer);
}

  /* This says how many bytes we wrote. */
void bll_out_count(struct session * s, size_t octet_count) {
  int inx;
  while (0 < octet_count) {
    PRE(0 < s->out_buffer.vec_count);
    if (octet_count < s->out_buffer.iovec[0].iov_len) {
      /* Did not write entire first buffer */
      s->out_buffer.iovec[0].iov_len -= octet_count;
      (s->out_buffer.iovec[0].iov_base) = ((char*)s->out_buffer.iovec[0].iov_base) + octet_count;
      octet_count = 0;
    } else {
      octet_count -= s->out_buffer.iovec[0].iov_len;
      for (inx = 0; inx < s->out_buffer.vec_count - 1; inx++) 
        s->out_buffer.iovec[inx] = s->out_buffer.iovec[inx+1];
      s->out_buffer.vec_count -= 1;
    }
  }
}

  /* As far as incoming buffers go, first we read into in_head_buf 
     until we get 80 characters or a \r\n. Then we read into the 
     frame until we get all the characters there. Then we read
     into end_buf until we get END\r\n. Basically, there's really
     only ever one buffer. */

struct beep_iovec * bll_in_buffer(struct session * s) {
  PRE(s != NULL);
  if (0 < s->status) 
    return NULL;

  if (s->tuning != -1) {
    struct channel * c;
    PRE(s != NULL);
    c = s->channel;
    while (c && c->channel_number != s->tuning) 
      c = c->next;
    ASSERT(c != NULL);
    if (c->sent == NULL)
        return NULL;
  }

  return &(s->in_buffer);
}

  /* This tells us how many bytes were just read. This really handles
     everything.  */
void bll_in_count(struct session * s, size_t size) {
  char * nl;

  PRE(size <= s->in_buffer.iovec[0].iov_len);
  s->in_buffer.iovec[0].iov_base = (char*) (s->in_buffer.iovec[0].iov_base) + size;
  s->in_buffer.iovec[0].iov_len -= size;
  ((char*)(s->in_buffer.iovec[0].iov_base))[0] = '\0';

  if (s->read_frame == NULL) {
    /* I'm reading the header */
    nl = strchr(s->read_buf, '\n');
    if (nl == NULL && s->in_buffer.iovec[0].iov_len==0) {
      /* Read the whole buffer and didn't find newline */
      FAULT(s, 8, "Header line too long");
    } else if (nl != NULL) {
      /* read the whole first line */
      if ('S' == s->read_buf[0]) {
        parse_seq(s);
        if (s->status <= 0) bll_in_count(s, 0); /* Check to see if there's another header */
          /* Should probably be a loop instead of recursive, 
	     but it's just parsing the headers from an 80-char buffer */
      } else {
        parse_head(s);
	if (s->status <= 0) bll_in_count(s, 0);
      }
    }
  } else {
    /* I'm reading into a frame */
    if (0 == s->in_buffer.iovec[0].iov_len) {
      /* finished reading frame */
      place_frame(s);
      s->in_buffer.iovec[0].iov_base = s->read_buf;
      s->in_buffer.iovec[0].iov_len = sizeof(s->read_buf)-2;
      s->read_buf[0] = '\0';
    } else {
      /* I'm still reading payload, so don't do anything more */
    }
  }
}

static char * N(char * t) {
  /* Because printf is too stupid to handle this */
  if (t==NULL)
    return ">>NULL<<";
  else
    return t;
}

void bll_frame_debug_print(struct frame * f, FILE * file) {
  if (f==NULL) {
    fprintf(file, "      NULL FRAME POINTER\n");
    return;
  }
  fprintf(file, "      FRAME at %lX (session=%lX):\n", (long) f, (long) f->session);
  fprintf(file, "      Next on channel: %lX\n", (long) f->next_on_channel);
  fprintf(file, "      Next in message: %lX\n", (long) f->next_in_message);
  fprintf(file, "      msg_type='%c' more='%c'\n", f->msg_type, f->more);
  fprintf(file, "      channel_number=%ld\n", f->channel_number);
  fprintf(file, "      message_number=%ld\n", f->message_number);
  fprintf(file, "      answer_number =%ld\n", f->answer_number);
  fprintf(file, "      size=%ld, alloc=%ld\n", f->size, f->alloc);
  fprintf(file, "      payload=>>>%s<<<\n", N(f->payload));
}

void bll_channel_debug_print(struct channel * c, FILE * file) {
  struct frame * f;
  struct uamn * a;

  fprintf(file, "    CHANNEL at %lX:\n", (long) c);
  fprintf(file, "    next %lX\n", (long) c->next);
  fprintf(file, "    chan_number=%ld\n", c->channel_number);
  fprintf(file, "    channel_info=%lX\n", (long) c->channel_info);
  fprintf(file, "    priority=%d\n", c->priority);
  f = c->in_frame;
  fprintf(file, "    in_frame:\n");
  bll_frame_debug_print(f, file);
  while (f) {
    f = f->next_on_channel;
    bll_frame_debug_print(f, file);
  }
  f = c->out_frame;
  fprintf(file, "    out_frame:\n");
  bll_frame_debug_print(f, file);
  while (f) {
    f = f->next_on_channel;
    bll_frame_debug_print(f, file);
  }
  fprintf(file, "    commit_frame:\n");
  bll_frame_debug_print(c->commit_frame, file);

/*fprintf(file, "    apparent out window: %ld\n", c->apparent_out_window); SR - unimplimented*/
  fprintf(file, "    cur_out_msg type='%c', number=%ld\n", c->cur_out_msg_type, c->cur_out_msg_number);
  fprintf(file, "    cur_in_seq=%lu\n", c->cur_in_seq);
  fprintf(file, "    max_in_seq=%lu\n", c->max_in_seq);
  fprintf(file, "    cur_in_buf=%lu\n", c->cur_in_buf);
  fprintf(file, "    max_in_buf=%lu\n", c->max_in_buf);
  fprintf(file, "    cur_out_seq=%lu\n", c->cur_out_seq);
  fprintf(file, "    max_out_seq=%lu\n", c->max_out_seq);

  a = c->sent;
  fprintf(file, "    SENT messages: %s\n", (a==NULL)?"NONE":"");
  while (a) {
    fprintf(file, "      Sent %ld msg_more='%c' rpy_more='%c'\n", a->luamn, a->msg_more, a->rpy_more);
    a = a->next;
  }

  a = c->rcvd;
  fprintf(file, "    RCVD messages: %s\n", (a==NULL)?"NONE":"");
  while (a) {
    fprintf(file, "      Rcvd %ld msg_more='%c' rpy_more='%c'\n", a->luamn, a->msg_more, a->rpy_more);
    a = a->next;
  }

}

void bll_session_debug_print(struct session * s, FILE * f) {
  int i;
  struct channel * c;

  fprintf(f, "Start of dump, session at %lX\n", (long) s);
  fprintf(f, "  in_buffer.vec_count=%d\n", s->in_buffer.vec_count);
  for (i = 0; i < s->in_buffer.vec_count; i++) {
    fprintf(f, "  in_buffer.iov_len[%d] = %ld\n", i,
	    (long)s->in_buffer.iovec[i].iov_len);
    fprintf(f, "  in_buffer.iov_base[%d] = %lX = '%s'\n", i,
	    (unsigned long) s->in_buffer.iovec[i].iov_base, (char *)s->in_buffer.iovec[i].iov_base);
  }
  fprintf(f, "  out_buffer.vec_count=%d\n", s->out_buffer.vec_count);
  for (i = 0; i < s->out_buffer.vec_count; i++) {
    fprintf(f, "  out_buffer.iov_len[%d] = %ld\n", i,
	    (long)s->out_buffer.iovec[i].iov_len);
    fprintf(f, "  out_buffer.iov_base[%d] = %lX = '%s'\n", i,
	    (unsigned long) s->out_buffer.iovec[i].iov_base, (char *)s->out_buffer.iovec[i].iov_base);
  }
  fprintf(f, "  status=%d. status_channel=%ld. status_text=%s\n",
	  s->status, s->status_channel, N(s->status_text));
  fprintf(f, "  memory_limit=%ld, memory_used=%ld\n", 
	  s->memory_limit, s->memory_used);
  fprintf(f, "  frame_pre=%d, frame_post=%d\n", s->frame_pre, s->frame_post);
  fprintf(f, "  IL=%c\n", s->IL);
  fprintf(f, "  server_name='%s'\n", N(s->server_name));
  fprintf(f, "  Offered profiles: %s\n", (s->offered_profiles == NULL ? "NULL" : ""));
  for (i = 0; s->offered_profiles != NULL && s->offered_profiles[i] != NULL; i++)
    fprintf(f, "    %s\n", s->offered_profiles[i]);
  fprintf(f, "  Greeting error: %lX\n", (long) s->greeting_error);
  fprintf(f, "  Features: %s\n", N(s->features));
  fprintf(f, "  Localize: %s\n", N(s->localize));
  fprintf(f, "  Session info: %lX\n", (long) s->session_info);
  fprintf(f, "  SEQ buffer: '%s'\n", s->seq_buf);
  fprintf(f, "  HEAD buffer: '%s'\n", s->head_buf);
  fprintf(f, "  READ buffer: '%s'\n", s->read_buf);
  fprintf(f, "  READ SEQ: %ld\n", s->read_seq);
  fprintf(f, "  READ FRAME:\n"); bll_frame_debug_print(s->read_frame, f);
  fprintf(f, "  Channels:\n");
  c = s->channel;
  bll_channel_debug_print(c, f);

}

struct frame * blu_frame_create(struct session * s, size_t size) {
  struct frame * f;
  PRE(s != NULL);
  PRE(0 <= size);
  if (0 < s->memory_limit && s->memory_limit < (long)size + s->memory_used) {
    FAULT(s, -2, "In blu_frame_create");
    return NULL;
  }
  f = frame_create(s,size);
  if (f == NULL) {
    FAULT(s, -1, "In blu_frame_create");
    return NULL;
  }
  f->alloc = size;
  s->memory_used += size;
  return f;
}

/*TODO - use this everywhere */
static
struct channel* channel_by_no(struct session * s, long channel_number)
{
  struct channel* c;

  PRE(s != NULL);
  PRE(0 <= channel_number);

  c = s->channel;

  ASSERT(c != NULL);

  while (c && c->channel_number != channel_number) 
    c = c->next;

  return c;
}

long blu_local_window_set(struct session * s, long channel_number, long window) {
  long old_window;
  struct channel * c;

  PRE(-1 <= window);
  PRE(window < MAXWIN);

  c = channel_by_no(s, channel_number);

  if (c == NULL) {
    s->status_channel = channel_number;
    FAULT(s, -4, "Setting window on closed channel");
    return 0;
  }
  old_window = c->max_in_buf;
  if (0 <= window) {
    c->max_in_buf = window;
    /* We may be generating an SEQ here */
    (*(s->notify_lower))(s, 3);
  }
  return old_window;
}

void blu_max_frame_size_set(struct session * s, int frame_size) {
  PRE(s != NULL);
  PRE(-1 <= frame_size);
  PRE(frame_size < MAXWIN);

  s->max_frame_size = frame_size;
}

void blu_frame_destroy(struct frame * f) {
  struct session * s;
  int is_in_window;

  PRE(f != NULL);
  is_in_window = (f->alloc < 0);
  s = f->session;
  frame_destroy(f, NULL, s);
  if (is_in_window) {
    /* We may be generating an SEQ here */
    (*(s->notify_lower))(s, 3);
  }
}

long blu_peer_window_current_get(struct session * s, long channel) {
  struct channel * c;
  PRE(s != NULL);
  c = s->channel;
  while (c && c->channel_number != channel) 
    c = c->next;
  if (c == NULL) {
    s->status_channel = channel;
    FAULT(s, -4, "blu_peer_window_current_get for closed channel");
    return 0;
  } else {
    return c->max_out_seq - c->cur_out_seq;
  }
}

struct frame * blu_frame_read(struct session * s, long channel_number) {
  struct frame * f;
  struct channel * c;
  PRE(s != NULL);
  PRE(-1 <= channel_number);
  for (c = s->channel; c != NULL; c = c->next) {
    if (c->channel_number == channel_number || 
        (channel_number == -1 && c->in_frame != NULL)) {
      break;
    }
  }
  if (0 <= channel_number && c == NULL) {
    s->status_channel = channel_number;
    FAULT(s, -4, "blu_frame_read on closed channel");
    return NULL;
  }
  f = c->in_frame;
  if (f != NULL) {
    c->in_frame = f->next_on_channel;
  }
  /* Then, if the status has gone higher, let them know. */
  call_notify_upper(s, c);
  /* Then return the answer */
  return f;
}

  /* Set a pointer associated with a given channel
     on a given session, otherwise unmanaged. */
void blu_channel_info_set(struct session * s, long channel_number,void * user_info) {
  struct channel * c;
  PRE(s != NULL);
  PRE(0 <= channel_number);
  for (c = s->channel; c != NULL && c->channel_number != channel_number; c = c->next)
    /* Just find it */;
  if (c==NULL) {
    s->status_channel = channel_number;
    FAULT(s, -4, "blu_channel_info_set on closed channel");
    return;
  }
  c->channel_info = user_info;
}


  /* Get a pointer associated with a given channel
     on a given session, otherwise unmanaged.
     Returns NULL if never set. */
void * blu_channel_info_get(struct session * s, long channel_number) {
  struct channel * c;
  PRE(s != NULL);
  PRE(0 <= channel_number);
  for (c = s->channel; c != NULL && c->channel_number != channel_number; c = c->next)
    /* Just find it */;
  if (c==NULL) {
    s->status_channel = channel_number;
    FAULT(s, -4, "blu_channel_info_get on closed channel");
    return NULL;
  }
  return c->channel_info;
}

  /* Sets a pointer in the session that is
     otherwise ignored by the BEEP library. */
void blu_session_info_set(struct session * s, void * user_info) {
  PRE(s != NULL);
  s->session_info = user_info;
}

  /* Returns the pointer set by blu_session_info_set.
     Returns NULL if never set. */
void * blu_session_info_get(struct session * s) {
  PRE(s != NULL);
  return s->session_info;
}

#if 0
/*SR - unimplimented*/
  /* This returns the largest recent outgoing window size. */
long blu_peer_window_maximum_get(struct session * s, long channel_number) {
  struct channel * c;
  PRE(s != NULL);
  PRE(0 <= channel_number);
  for (c = s->channel; c != NULL && c->channel_number != channel_number; c = c->next)
    /* Just find it */;
  if (c==NULL) {
    s->status_channel = channel_number;
    FAULT(s, -4, "blu_get_window_maximum on closed channel");
    return 0;
  }
  return c->apparent_out_window;
}
#endif

  /* This returns true if the frame is a match, false otherwise */
static int match_frame(struct frame * f, char msg_type, long msgno, long ansno, char more) {
  PRE(f != NULL);
  if (msg_type != ' ' && msg_type != f->msg_type) return 0;
  if (msgno != -1 && msgno != f->message_number) return 0;
  if (ansno != -1 && ansno != f->answer_number) return 0;
  if (more == '.' && f->more != '.') return 0;
  return 1;
}

  /* This obtains the first or all frames that match the query */
struct frame * blu_frame_query(
    struct session * s, long channel_number, 
    char msg_type, long msgno, long ansno, char more) {
  struct channel * c;
  struct frame * f;
  struct frame * p; /* previous frame */
  struct frame * first; /* First in message */
  struct frame * prev;  /* Previous in message */
  int found;

  if (channel_number == -1) {
    c = s->channel;
    while (c != NULL) {
      if (c->channel_number != 0) {
        f = blu_frame_query(s, c->channel_number, msg_type, msgno, ansno, more);
        if (f != NULL) {
	  call_notify_upper(s, c);
	  return f;
	}
      }
    }
    return NULL; /* if we didn't return earlier */
  }

  /* Otherwise, we want a specific channel. Find it. */
  c = s->channel;
  while (c != NULL && c->channel_number != channel_number) {
    c = c->next;
  }
  if (c == NULL) {
    /* @$@$ Do we want to flag this as an error? */
    return NULL;
  }

  /* OK, now see if any frame matches. If so, get all matches. */
  p = NULL; f = c->in_frame; found = 0;
  while (f != NULL && !match_frame(f, msg_type, msgno, ansno, more)) {
    p = f; f = f->next_on_channel;
  }
  if (f == NULL) {
    /* No match at all. */
    return NULL;
  }

  if (more != '.') {
    /* Here we found a frame, p is the previous, and we don't want the whole message */
    if (p == NULL) 
      c->in_frame = f->next_on_channel;
    else 
      p->next_on_channel = f->next_on_channel;
    f->next_on_channel = NULL;
    f->next_in_message = NULL;
    call_notify_upper(s, c);
    return f;
  }
  /* OK. Here we found that we have a frame, but we want lots of frames. What a mess. */
  msg_type = f->msg_type;     /* These three make sure we don't match frames from different messages */
  msgno = f->message_number;
  ansno = f->answer_number;
  first = NULL; prev = NULL;
  p = NULL; f = c->in_frame; 
  while (f != NULL) {
    if (match_frame(f, msg_type, msgno, ansno, '*')) {
      /* first, link it in */
      if (first == NULL) first = f;
      if (prev != NULL) prev->next_in_message = f;
      prev = f;
      if (p != NULL) {
        p->next_on_channel = f->next_on_channel;
        p = f;
      } else {
        ASSERT(c->in_frame == f);
        c->in_frame = f->next_on_channel;
      }
    if (f->more == '.') break; /* stop after a full message */
    } else {
      p = f;
    }
    f = f->next_on_channel;
  }
  for (f = first; f != NULL; f = f->next_in_message) f->next_on_channel = NULL; /* avoid accident */
  call_notify_upper(s, c);
  return first;
}

  /* This aggregates frames (duh) */
struct frame * blu_frame_aggregate(struct session * s, struct frame * in) {
  long size;
  struct frame * f;
  struct frame * out;
  char * copy_point;

  if (in == NULL) return NULL; /* Probably a handy shortcut */
  for (size = 0, f = in; f != NULL; f = f->next_in_message)
    size += f->size;
  out = blu_frame_create(s, size);
  if (out == NULL) return NULL;
  for (f = in; f->next_in_message != NULL; f = f->next_in_message)
    /* Just point f at the last frame */;
  out->next_in_message = NULL;
  out->next_on_channel = NULL;
  out->msg_type = f->msg_type;
  out->channel_number = f->channel_number;
  out->message_number = f->message_number;
  out->answer_number = f->answer_number;
  out->more = f->more;

  copy_point = out->payload;
  f = in;
  while (f != NULL) {
    memmove(copy_point, f->payload, f->size);
    copy_point += f->size;
    f = f->next_in_message;
  }
  out->payload[out->size] = '\0'; /* We know there's a bit of slack there */
  return out;
}


  /* This queues a message to start one of the 
     indicated profiles on the indicated channel.
     If channel==-1, an approrpriate channel number 
     is chosen. The channel number is returned. */
long blu_channel_start(
    struct session * s,
    struct chan0_msg * chan0) {
  long channel_number;
  struct channel * c;
  struct frame * f;
  struct chan0_msg cz;  /* Note, not a pointer, but really allocated */

  PRE(s != NULL);
  PRE(chan0 != NULL);
  PRE(chan0->profile != NULL);

  channel_number = chan0->channel_number;

  /* auto-choose a channel number that doesn't collide with a manually chosen one */
  while (channel_number == -1 && s->next_channel < 1000000000) {
    /* Let's not try to fix the case of more than a billion channels opened @$@$ */
    channel_number = s->next_channel;
    s->next_channel += 2;
    for (c = s->channel; c != NULL; c = c->next) {
      if (channel_number == c->channel_number)
        channel_number = -1;
    }
  }
  /* ensure that a manually chosen channel number doesn't collide with an existing channel */
  for (c = s->channel; c != NULL; c = c->next) {
    if (channel_number == c->channel_number) {
      s->status_channel = channel_number;
      FAULT(s, 7, "Channel number already in use in blu_channel_start");
      return -1;
    }
  }
  if (channel_number <= 0 ||
      (s->IL == 'I' && (channel_number&1)==0) ||
      (s->IL == 'L' && (channel_number&1)==1)) {
    s->status_channel = channel_number;
    FAULT(s, 7, "Illegal channel in blu_channel_start");
    return -1;
  }
  c = channel_create(s, channel_number);
  if (c == NULL) {
    s->status_channel = channel_number;
    FAULT(s, -1, "Out of memory trying to create channel (1)");
    return -1;
  }

  bump_next_message(s);

  cz.session = s;
  cz.channel_number = channel_number;
  cz.message_number = s->next_message;
  cz.op_ic = 'i'; /* Not really an indication yet, but ... */
  cz.op_sc = 's';
  cz.profile = chan0->profile;
  cz.error = NULL;
  cz.localize = NULL; /* @$@$ ??? */
  cz.features = NULL; /* @$@$ ??? */
  cz.server_name = chan0->server_name;
  f = chan0_msg_fmt(s, &cz);
  if (f == NULL) {
    channel_destroy(s, c);
    /* FAULT(s, -1, "Out of memory trying to create channel (2)"); */
    return -1;
  }
  c->c0_message_number = f->message_number;
  c->status = 'S';
  c->channel_number = channel_number;
  blu_frame_send(f);
  return channel_number;
}

  /* This queues a message to close the indicated channel.
     If channel==0, this closes the entire session. */
void blu_channel_close(
    struct session * s,
    long channel_number,
    struct diagnostic * error) {
  struct channel * c;
  struct frame * f;
  struct chan0_msg cz;  /* Note, not a pointer, but really allocated */

  PRE(s != NULL);
  PRE(error != NULL);

  for (c = s->channel; c != NULL && c->channel_number != channel_number; c = c->next)
    /* Just look */ ;
  if (c == NULL) {
    s->status_channel = channel_number;
    FAULT(s, 7, "Closing channel that isn't open");
    return;
  }
  if (c->sent != NULL) {
    /* Outstanding unanswered messages. Fail for now. */
    s->status_channel = channel_number;
    FAULT(s, 7, "Closing channel with outstanding MSGs unanswered");
    return;
  }

  bump_next_message(s);

  cz.session = s;
  cz.channel_number = channel_number;
  cz.message_number = s->next_message;
  cz.op_ic = 'i'; /* Again, not really an indication, but... */
  cz.op_sc = 'c';
  cz.profile = NULL;;
  cz.error = error;
  cz.localize = NULL; 
  cz.features = NULL;
  cz.server_name = NULL;
  f = chan0_msg_fmt(s, &cz);
  if (f == NULL) {
    s->status_channel = channel_number;
    FAULT(s, -1, "Out of memory trying to close channel");
    /* @$@$ Is this recoverable, really? */
    return;
  }
  if (channel_number == 0) {
    for (c = s->channel; c != NULL; c = c->next) {
      if (c->c0_message_number != -1) {
        s->status_channel = c->channel_number;
        FAULT(s, 7, "Close 0 while close channel pending");
        return;
      }
    }
    s->close_status = f->message_number;
    blu_frame_send(f);
    for (c = s->channel; c != NULL; c = c->next) {
      c->c0_message_number = f->message_number;
      c->status = 'C';
    }
  } else {
    c->c0_message_number = f->message_number;
    c->status = 'C';
    blu_frame_send(f);
  }
}

  /* This parses and returns the next incoming message on
     channel 0, if any. It returns NULL if none.
     (Definitely needs some serious refactoring :-)
     The next message is one of
     - start indication (peer sent a <start> element)
     - start confirmation (peer answers <start> element)
     - close indication (peer sent a <close> element)
     - close confirmation (peer answers <close> element)
     A start indication will have profile filled in.
     A start confirmation will have profile or error or both filled in;
         if profile is filled in and error is NULL, the open was good;
         if profile is NULL and error is present, the channel did not open;
         if profile and error are both present, the channel opened but
            the piggybacked request returned an <error> element.
     A close indication will have error filled in.
     A close confirmation *may* have error filled in, indicating
         the channel is not closed.
     If it's an indication, it *must* be returned via 
       blu_chan0_reply. If it's a confirmation, it *must*
       be returned via blu_chan0_destroy.
     */

struct chan0_msg * blu_chan0_in(struct session * s) {
  struct chan0_msg * cz;
  struct channel * c;
  struct frame * f;

  if (NULL == (f = blu_frame_query(s, 0, BLU_QUERY_ANYTYPE, BLU_QUERY_ANYMSG,
				   BLU_QUERY_ANYANS, BLU_QUERY_COMPLETE)))
    return NULL;

  cz = chan0_msg_parse(s, f);
  blu_frame_destroy(f);

  if (NULL == cz) {
    return NULL;
  } 

  if (cz->op_sc == 'e') {
    /* Whoops. Parser can't distinguish between an error
     * in response to a <start> and an error in response
     * to a <close>, so it just says "error" and we look
     * up which it was. */
    /* SR - double whoops. Lookup below segfaulted when <error> is received
     * instead of a greeting.  I don't want to change the logic much until I
     * understand it better, but here I try and short-circuit this case.
     */
    c = s->channel;
    while (c != NULL && c->c0_message_number != cz->message_number)
      c = c->next;

    if(c == NULL && !s->remote_greeting &&
	cz->channel_number == 0 && cz->message_number == 0) {
      /* it's a greeting confirmation, with an error */
      cz->op_sc = 'g';
    } else {

    if (c == NULL) {
      FAULT(s, 8, "<error> on chan0 with no request");
      return NULL;
    }
    /* Here we found what we're answering. */
    if (c->status == 'S') {
      cz->op_sc = 's';
    } else if (c->status == 'C') {
      cz->op_sc = 'c';
    } else {
      FAULT(s, 8, "You can't get there from here.");
      return NULL;
    }
    cz->op_ic = 'c';
    }
  }

  if (cz->op_sc == 's') {
    if (cz->op_ic == 'i') {
      /* Start indication */
      if ((s->IL == 'I') == (cz->channel_number & 1)) {
        s->status_channel = cz->channel_number;
        FAULT(s, 8, "Attempt to start wrong-parity channel number");
        blu_chan0_destroy(cz);
        return NULL;
      }
      if (blu_channel_status(s, cz->channel_number) != 0) {
        s->status_channel = cz->channel_number;
        FAULT(s, 8, "Attempt to start started channel");
        blu_chan0_destroy(cz);
        return NULL;
      }
      c = channel_create(s, cz->channel_number);
     if (c == NULL) {
       s->status_channel = cz->channel_number;
       FAULT(s, -1, "Out of memory trying to create channel (3)");
       blu_chan0_destroy(cz);
       return NULL;
     }
      c->status = 'S'; c->c0_message_number = cz->message_number;
    } else {
      /* Start confirmation */
      cz->op_ic = 'c'; /* We don't pass back 'e' here */
      c = s->channel;
      while (c != NULL && c->c0_message_number != cz->message_number)
        c = c->next;
      if (c == NULL || c->status != 'S' || 
          c->c0_message_number != cz->message_number) {
        s->status_channel = cz->channel_number;
        FAULT(s, 8, "Start confirmation with no request");
        blu_chan0_destroy(cz);
        return NULL;
      }
      /* start confirmations need to know the channel which was started */
      cz->channel_number = c->channel_number;
      if (cz->error == NULL) {
        /* Finished starting - go for it */
        c->status = ' '; c->c0_message_number = -1;
      } else {
        /* start failed */
        channel_destroy(s, c);
      }
    }
  } else if (cz->op_sc == 'c') {
    if (cz->op_ic == 'i') {
      /* Close indicaton */
      return cz; /* I don't think we actaully need to do anything here. */
      /* @$@$ Actually, we probably want to mark it as half closed in a
              different way, so we don't get two <close> for the same channel. */
    } else {
      /* Close confirmation */
      cz->op_ic = 'c'; /* We don't pass back 'e' here */
      if (cz->message_number == s->close_status) {
        /* We just confirmed a channel-zero close */
        if (cz->error != NULL) {
          /* Could not close channel zero */
          for (c = s->channel; c != NULL; c = c->next) {
            c->status = ' '; c->c0_message_number = -1;
          }
          s->close_status = -1;
        } else {
          /* Close of channel zero successful */
          while (s->channel)
            channel_destroy(s, s->channel);
        }
      } else {
        /* Confirm of individual channel close */
        c = s->channel;
        while (c != NULL && c->c0_message_number != cz->message_number)
          c = c->next;
        if (c == NULL || c->status != 'C' || 
            c->c0_message_number != cz->message_number) {
          s->status_channel = cz->channel_number;
          FAULT(s, 8, "Close confirmation with no request");
          blu_chan0_destroy(cz);
          return NULL;
        }
        if (cz->error != NULL) {
          /* close failed */
          c->status = ' '; c->c0_message_number = -1;
        } else {
          /* Finished closing - go for it */
          channel_destroy(s, c);
        }
	/* close confirmation or error needs to know the channel */
	cz->channel_number = c->channel_number;
      }
    }
  } else {
    /* Must be greeting */
    ASSERT(cz->op_sc == 'g');
    /* SR - I don't know what the history of this code is, but its obviously
     * been heavily hacked up. The comments don't make sense anymore
     * (remote_greeting is never referenced anywhere, a good thing because
     * doing so would segf, its destroyed by our caller!), also it looks like
     * there was support for a while for saving the greeting information, but
     * all references to that have been commented out of the code base. */
    s->remote_greeting = cz; /* This gets disposed at session_destroy */
    if (cz->error != NULL) {
      /* We got an error, not a greeting */
      s->greeting_error = cz->error;
      FAULT(s, 3, "Received <error> instead of greeting");
    } else {
      /* We got a real greeting */
      /* 
      struct profile * p;
      int pc; *//* profile count */
      /*
      for (pc = 0, p = cz->profile; p != NULL; pc += 1, p = p->next)
      */
      /* Just count */ /*;
      s->offered_profiles = (char**)(*(s->malloc))((pc+1)*sizeof(p));
      if (s->offered_profiles == NULL) {
	FAULT(s, 5, "Out of memory handling greeting");
	return NULL;
      }
      for (pc = 0, p = cz->profile; p != NULL; pc += 1, p = p->next) 
	s->offered_profiles[pc] = p->uri;
      s->offered_profiles[pc] = NULL;
      s->features = cz->features;
      s->localize = cz->localize;
      */
      (void) bll_out_buffer(s);
    }
    (*(s->notify_lower))(s, 3);
    /* return NULL; */ /* Never return greetings */
  }
  return cz;
}

void blu_chan0_reply(
    struct chan0_msg * indication,
    struct profile * profile,
    struct diagnostic * error) {
  struct session * s;
  struct chan0_msg cz;
  struct channel * c;
  struct frame * f;

  PRE(indication != NULL);
  PRE(profile == NULL || error == NULL);
  PRE(!(indication->op_sc == 's' && error == NULL && profile == NULL));
  PRE(!(indication->op_sc == 'c' && profile != NULL));

  s = indication->session;
  if (indication->op_ic != 'i') {
    s->status_channel = indication->channel_number;
    FAULT(s, 7, "blu_chan0_reply to confirmation");
    /* @$@$ May induce a memory leak, but better than a core dump */
    blu_chan0_destroy(indication);
    return;
  }

  c = s->channel;
  while (c != NULL && c->channel_number != indication->channel_number)
    c = c->next;
  if (c == NULL) {
    s->status_channel = indication->channel_number;
    FAULT(s, 7, "blu_chan0_reply no such channel");
    blu_chan0_destroy(indication);
    return;
  }

  if (indication->op_sc == 's') {
    /* Replying to a start */
    cz.session = indication->session;
    cz.channel_number = indication->channel_number;
    cz.message_number = indication->message_number;
    cz.op_ic = 'c'; cz.op_sc = 's';
    cz.profile = profile; cz.error = error;
    cz.features = NULL; cz.localize = NULL; cz.server_name = NULL; /* @$@$ ??? */
    if (error == NULL) {
      /* Finish opening the channel */
      c->status = ' '; c->c0_message_number = -1;
      if (s->server_name == NULL && indication->server_name != NULL) {
        s->server_name = s->malloc(strlen(indication->server_name) + 1);
	if (s->server_name==NULL) {
          channel_destroy(s, c);
	  FAULT(s, 5, "Out of memory copying server_name");
	} else {
	  strcpy(s->server_name, indication->server_name);
	}
      }
    } else {
      channel_destroy(s, c);
    }
  } else {
    /* Replying to a close */
    cz.session = indication->session;
    cz.channel_number = indication->channel_number;
    cz.message_number = indication->message_number;
    cz.op_ic = 'c'; 
    if( error != NULL )
        cz.op_sc = 'e';
    else
        cz.op_sc = 'c';
    cz.profile = NULL; cz.error = error;
    cz.features = NULL; cz.localize = NULL; cz.server_name = NULL; /* @$@$ ??? */
    if (error != NULL) {
      /* Don't close channel 0 */
      c->status = ' '; c->c0_message_number = -1;
    } else {
      /* First make sure it's empty @$@$ */
      c->status='C';
      blu_chan0_destroy(indication);
      f = chan0_msg_fmt(s, &cz);
      if (f != NULL)  
          blu_frame_send(f); 
      /* Don't destroy channel 0 - wait for channel 0 status of 5 - then session destroy */
      if (cz.channel_number != 0)
          channel_destroy(s, c);
      return; 
    }
  }
  blu_chan0_destroy(indication);
  f = chan0_msg_fmt(s, &cz);
  if (f == NULL) return; /* Out of memory */
  blu_frame_send(f);
}

void blu_chan0_destroy(struct chan0_msg * cz) {
  struct session * s;
  PRE(cz != NULL);
  s = cz->session;
  if (s != NULL) {
    (*(s->free))(cz);
  }
}

void bll_session_destroy(struct session * s) {
  /*
  if (s->remote_greeting)
    (*(s->free))(s->remote_greeting);
  if (s->offered_profiles)
    (*(s->free))(s->offered_profiles);
  */
  if (s->read_frame) 
    (*(s->free))(s->read_frame);
  while (s->channel)
    channel_destroy(s, s->channel);
  if (s->server_name)
    (*(s->free))(s->server_name);
  (*(s->free))(s);
  return;
}

int blu_channel_status(
    struct session * sess,
    long channel_number) {
  struct channel * c;
  struct frame * f;

  PRE(sess != NULL);
  PRE(0 <= channel_number);
  for (c = sess->channel; c != NULL && c->channel_number != channel_number; c = c->next)
    /* Just search */ ;
  if (c == NULL) return 0; /* Channel is closed. */
  if (c->status == 'S') return 1; /* Channel is starting */
  if (c->status == 'C') return 2; /* Channel is closing */

  /* OK, here's the scoop. 
     If there's a MSG waiting to go out, *or* there's a "sent", then there are unanswered MSGs. 
     If there's a RPY waiting to go out, or a "rcvd", then there's unsent replies. 
     In other words, when you commit the frame to send, that's when it goes in
     "sent", and when it's queued as an in_frame, that's when it goes in "rcvd". */

  if (c->sent != NULL)  return 3; /* Channel is waiting for replies */
  for (f = c->out_frame; f != NULL; f = f->next_on_channel)
    if (f->msg_type == 'M') return 3; /* Channel is waiting to send MSGs, and therefore get replies */
  for (f = c->in_frame; f != NULL; f = f->next_on_channel)
    if (f->msg_type != 'M') return 3; /* Channel is waiting to consume replies */
  if (c->cur_out_msg_type == 'M') return 3;

  if (c->rcvd != NULL)  return 4; /* Channel is waiting to send replies */
  if (c->out_frame != NULL) return 4; /* Must be a reply, or we would have returned earlier */
  if (c->commit_frame != NULL) return 4; /* Again, must be a reply */
  for (f = c->in_frame; f != NULL; f = f->next_on_channel)
    if (f->msg_type == 'M') return 4; /* Channel is waiting to consume incoming MSGs */

  /* I think this checks it all. */
  ASSERT(c->commit_frame == NULL);
  ASSERT(c->sent == NULL);
  ASSERT(c->rcvd == NULL);
  ASSERT(c->in_frame == NULL);
  ASSERT(c->out_frame == NULL);
  return 5;

}

void blu_tuning_reset(struct session * s, long channel) {
  PRE(s != NULL);
  s->tuning = channel;
  /* Might want to have a check that channel==-1 or it is open */
}

/*
 * blu_channel_statusflag_get
 * blu_channel_statusflag_set
 *
 * These are a hack because the tuning reset stuff is not in CBEEP.c yet.
 */
char blu_channel_statusflag_get(struct session * sess, long channel_number)
{
  struct channel * c;

  PRE(sess != NULL);
  PRE(0 <= channel_number);
  for (c = sess->channel; 
       c != NULL && c->channel_number != channel_number; 
       c = c->next)
    /* Just search */ ;

  if (c) 
    return c->status;
  else 
    return (char)0;
}

void blu_channel_statusflag_set(struct session * sess, long channel_number, 
				char stat)
{
  struct channel * c;

  PRE(sess != NULL);
  PRE(0 <= channel_number);
  for (c = sess->channel; 
       c != NULL && c->channel_number != channel_number; 
       c = c->next)
    /* Just search */ ;

  if (c) 
    c->status = stat;

  return;
}

/*
 *
 */
int blu_channel_frame_count(struct session * sess, long channel_number,
    char msg_type, char more) {
  int count = 0;
  struct frame * f;
  struct channel * c;
  PRE(sess != NULL);
  PRE(0 <= channel_number);
  for (c = sess->channel; c != NULL && c->channel_number != channel_number; c = c->next)
    /* Just search */ ;
  if (c == NULL) return 0; /* Channel is closed. */
  for (f = c->in_frame; f != NULL; f = f->next_on_channel) {
    if (msg_type != ' ' && msg_type != f->msg_type) continue;
    if (more == '.' && more != f->more) continue;
    count += 1;
  }
  return count;
}


