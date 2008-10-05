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
 * CBEEP.h
 *
 * This is the .h file for the BEEP library in C.
 *
 */

#ifndef __CBEEP_H__
#define __CBEEP_H__

#ifndef WIN32
#ifdef	NETBSD
#include <sys/types.h>
#endif
#include <sys/uio.h>
#endif
#include <stdio.h>

#if defined(__cplusplus)
extern "C"
{
#endif

/* First, the structures that are common to 
   both UpperAPI and LowerAPI */


  /* The session structure represents one session.
     In an OO implementation, this would be an object.
     It maintains all state for the session. There are
     no static variables in the library, but the 
     library is not reentrant for any given session.
     The session structure must be locked if threading
     might cause the library to be called inappropriately.
     */

struct session; /* This is opaque */

  /* This represents an error message */
/**
 * A structure for conveying a localized diagnostic message.
 *
 * @field code The numeric reason.
 * Consult \URL[Section 8 of RFC 3080]{http://www.beepcore.org/beepcore/docs/rfc3080.jsp#reply-codes}
 * for an (incomplete) list.
 *
 * @field lang The localization language used by the <i>message</i>
 *		   parameter.
 *
 * @field message A textual message corresponding to the <i>code</i> parameter.
 *
 * @see bp_diagnostic_new
 * @see bp_diagnostic_destroy
 **/
struct diagnostic {
  int code;
  char * lang;
  char * message;
};

/*
 *   Defines related to upper and lower level calls
 */
#define BLU_FRAME_TYPE_MSG            'M' 
#define BLU_FRAME_TYPE_RPY            'R' 
#define BLU_FRAME_TYPE_ANS            'A' 
#define BLU_FRAME_TYPE_NUL            'N' 
#define BLU_FRAME_TYPE_ERR            'E' 

#define BLU_QUERY_ANYCHANNEL        -1 
#define BLU_QUERY_ANYTYPE             ' ' 
#define BLU_QUERY_ANYMSG            -1
#define BLU_QUERY_ANYANS            -1
#define BLU_QUERY_COMPLETE            '.'
#define BLU_QUERY_PARTIAL             '*'

#define BLU_CHAN0_IC_INDICATION       'i'
#define BLU_CHAN0_IC_CONFIRM          'c'
#define BLU_CHAN0_IC_ERROR            'e'

#define BLU_CHAN0_SC_START            's'
#define BLU_CHAN0_SC_CLOSE            'c'

#define BLU_NOTEUP_FRAME	     1 /* A frame has arrived */
#define BLU_NOTEUP_MESSAGE	     2 /* A complete frame has arrived */
#define BLU_NOTEUP_QEMPTY	     3 /* The out-queue is empty */
#define BLU_NOTEUP_ANSWERED	     4 /* All sent MSGs answered */
#define BLU_NOTEUP_QUIESCENT	     5 /* All messages handled */
#define BLU_NOTEUP_WINDOWFULL	     6 /* A frame arrived which filled the window */

#define BLU_CHAN_STATUS_CLOSED       0 /* closed or does not exist. */
#define BLU_CHAN_STATUS_HOPEN        1 /* half started. */
#define BLU_CHAN_STATUS_HCLOSED      2 /* half closed. */
#define BLU_CHAN_STATUS_BUSY         3 /* busy. */
#define BLU_CHAN_STATUS_ANSWERED     4 /* all queued MSGs fully answered. */
#define BLU_CHAN_STATUS_QUIESCENT    5 /* No unanswered MSG/queued frames. */

#define BLU_CHAN_STATUSFLAG_RUNNING   ' '
#define BLU_CHAN_STATUSFLAG_CLOSING   'C'
#define BLU_CHAN_STATUSFLAG_STARTING  'S'

#define BLU_TUNING_RESET_ROLLBACK   -1

#define BEEP_REPLY_SUCESS          200
#define BEEP_REPLY_NOT_TAKEN       450
#define BEEP_REPLY_ABORTED         451
#define BEEP_REPLY_AUTH_FAIL_TEMP  454
#define BEEP_REPLY_ERR_SYNTAX      500
#define BEEP_REPLY_ERR_PARAM       501
#define BEEP_REPLY_PARAM_NOT_IMPL  504
#define BEEP_REPLY_AUTH_REQD       530
#define BEEP_REPLY_AUTH_TOOWEAK    535
#define BEEP_REPLY_AUTH_FAILED     537
#define BEEP_REPLY_NOT_AUTORIZED   538
#define BEEP_REPLY_NO_ACTION       550
#define BEEP_REPLY_INVALID_PARAM   553
#define BEEP_REPLY_FAIL_TRANS      554


/* * * * * Lower Level API * * * * * */

#ifdef WIN32
struct iovec {
    void* iov_base;
    size_t iov_len;
};
#endif

  /* This is manages in such a way that
     once an iovec is written, it is not changed
     except during a call to bll_out_count().
     Also, each iovec is filled in before
     vec_count is incremented. Hence, this
     structure is always in a consistant
     state. Hence, getting the pointer to it
     and passing it to writev() is fine. */
struct beep_iovec {
  int vec_count;     /* How many iovec's are filled in? */
  struct iovec iovec[10];  /* The iovecs for writing */
};


  /* This tells you what buffers are ready
     to be written from the session. A zero
     for vec_count means nothing waiting to write.
     This is valid only until you call bll_out_count().
     */
struct beep_iovec * bll_out_buffer(
  struct session * sess);

  /* This tells the session how many
     bytes you just wrote. 
     You must call bll_out_buffer between
     calls to bll_out_count().
     */
void bll_out_count(
  struct session * sess,
  size_t octet_count);  /* total octets written */


  /* This tells you what buffers to fill up. 
     A zero for vec_count means no buffers waiting
     to be read. */
struct beep_iovec * bll_in_buffer(
  struct session * sess);

  /* This tells the session how many octets you read. */
void bll_in_count(
  struct session * sess,
  size_t octet_count);  /* Total number of octets read */


  /* This tells you the status of the session. */
int bll_status(struct session * sess);
  /* 0 - Keep going. Nothing to see here. Move along.
     1 - Tuning reset finished. See bll_channel()
     2 - Session closed cleanly.
     3 - <error> instead of greeting received.
     4 - Reserved.
     5 - Out of memory, non-recoverable.
     6 - Other resource error.
     7 - Local profile error. See bll_channel()
     8 - Remote protocol error.
    -1 - Out of memory, recoverable.
    -2 - Tried to exceed set memory limits.
    -3 - Query for complete message with full but incomplete buffer.
    -4 - Ignorable local error. See bll_channel()
  */

  /* If bll_status() returned 1 or 7, this returns the
     channel number that caused that result. */
long bll_status_channel(struct session * sess);

  /* if bll_status() does not return 0, this returns
     an informative message for the developer. */
char * bll_status_text(struct session * sess);


  /* This tells the session how much extra memory it's allowed
     to allocate for frames (via blu_frame_create). I.e., this 
     number plus the sum of all the channel window sizes plus 
     8K * the number of advertised greetings plus overhead will 
     approximate the amount of memory requested at one time 
     by the session. It returns the amount of currently allocated 
     extra memory. */
long bll_memory_limit(
  struct session * session,
  long new_limit);


  /* Create a session. 
     If error!=NULL, the greeting indicates the session is refused.
     Otherwise, profiles is the NULL-terminated list of profiles to advertise.
     IL is 'I' if this is an initiator session, or 'L' if this is a listener.
       (The difference being in the channel numbers chosen.)
     `features' and `localize' are attributes to be sent in the greeting.
     `malloc' and `free' are pointers to functions for memory management.
     `notify_lower' and `notify_upper' are documented below.
     Returns NULL if session could not be created.
     */
struct session * bll_session_create(
  void *(*malloc)(size_t),
  void (*free)(void*),
  void (*notify_lower)(struct session *,int),
  void (*notify_upper)(struct session *,long,int),
  char IL,
  char * features,
  char * localize,
  char * * profile,
  struct diagnostic * error,
  void *user_data);

  /* Destroy a session.
     This frees all memory associated with the session, etc.
     It does not close channels or anything. It just frees resources.
     NB! Do not destroy frames after you've destroyed the session.
         It would be Bad.
  */
void bll_session_destroy(
  struct session * session);

  /* This is called by the BEEP library to indicate that
     the status of the Lower API has changed. Do not call
     any BEEP library operations from within this function.
     `operation' is one of
       1 - bll_status() value has changed.
       2 - bll_in_buffer()  has a larger `buffer_count'
       3 - bll_out_buffer() has a larger `buffer_count'
     OK. We're always ready to read, so we don't call 2.
  */
void notify_lower(
  struct session * session, 
  int operation);


  /* This is called by the BEEP library to indicate that
     the status of the Upper API has changed. Do not call
     any BEEP library operations from within this function.
     `operation' is one of
      1 - BLU_NOTEUP_FRAME
          A frame has arrived for the given channel
      2 - BLU_NOTEUP_MESSAGE
          A message has arrived for the given channel
          (I.e., a frame with more=='.')
          On channel 0, this means an indication 
          or confirmation is available.
      3 - BLU_NOTEUP_QEMPTY
          All queued outgoing frames for this channel 
          have been written.
      4 - BLU_NOTEUP_ANSWERED
          All sent MSGs have been answered in full.
          (Safe to initiate close, tuning reset, etc.)
      5 - BLU_NOTEUP_QUIESCENT
          Channel is quiescent.
          (Safe to confirm close, tuning reset, etc.)
      6 - BLU_NOTEUP_WINDOWFULL
          Receive window has filled up.
  */

void notify_upper(
  struct session * session,
  long channel,
  int operation);

/* * * * * * * Upper Level API * * * * * * * * */

/* doc++ note: only public fields are documented... */
/**
 * A structure for conveying a frame (or perhaps message).
 *
 * @field next_in_message A pointer to the next frame in this message,
 *	or <b>NULL</b>.
 *
 * @field msg_type Indicates the frame type, one of:
 *                <ul>
 *                <li>BLU_FRAME_TYPE_MSG</li>
 *                <li>BLU_FRAME_TYPE_RPY</li>
 *                <li>BLU_FRAME_TYPE_ANS</li>
 *                <li>BLU_FRAME_TYPE_NUL</li>
 *                <li>BLU_FRAME_TYPE_ERR</li>
 *                </ul>
 *
 * @field message_number Indicates the message number associated with the frame.
 *
 * @field answer_number  Indicates the answer number associated with the frame,
 *	meaningful only if <i>msg_type</i> is <b>BLU_FRAME_TYPE_ANS</b>.
 *
 * @field more Indicates if this frame completes a message, one of:
 *                <ul>
 *                <li>BLU_FRAME_COMPLETE</li>
 *                <li>BLU_FRAME_PARTIAL</li>
 *                </ul>
 *
 * @field size The number of octets in the payload.
 *
 * @field payload A pointer to the first octet in the payload.
 *
 * @see bpc_frame_read
 * @see bpc_query
 * @see bpc_frame_aggregate
 * @see bpc_frame_destroy
 **/
struct frame {
  struct session * session;
  struct frame * next_in_message;
  struct frame * next_on_channel;
  char msg_type; /* 'M', 'A', 'R', 'E', 'N' */
  long channel_number;
  long message_number;
  long answer_number;
  char more; /* '*' or '.' */
  long alloc; /* Internal memory management - fiddle not */
  long size; /* Size of payload */
  char * payload; /* Payload space */
  /* Here would be scatter/gather structures,
     in a future release, used if payload==NULL. */
  /* Reserved stuff comes here */
  /* Then payload points to here */
};


  /* This indicates a channel open event coming in. */
typedef struct chan0_msg CHAN0_MSG; 
struct chan0_msg {
  struct session * session;
  long channel_number;
  long message_number;

  char op_ic;                 /* 'i' for indication, 'c' for confirmation, 'e' for error */
  char op_sc;                 /* 'g' for greeting, 's' for start, 'c' for close */
  struct profile * profile;   /* Incoming profiles, or one selected profile */
  struct diagnostic * error;       /* NULL, or error structure. */

  long profileC;	      /* count of the number of profiles */
  char * features;            /* Features attribute for greeting */
  char * localize;            /* localize attibute for greeting */
  char * server_name;          /* server_name attribute for start  */
};

/* a specific structure for channel starts */

/* a specific structure for channel start replies
 * normally, we would just a diagnostic, but since the start reply
 * can piggy back binary data, we need another member for the
 * data and its length (since binary can't be readily determined as
 * a string).
 */
struct chan_start_rpy {
    struct diagnostic* error;  /* error element to return on error */
    void* data;                /* response data if there is some */
    long data_len;             /* length of response data */
};

/* structure containing elements for a greeting */
struct greeting {
    struct session* session;   /* session for greeting */
    struct profile* profiles;  /* profiles advertised on this greeting */
    long profileC;             /* number of profiles in the list */
    char* features;            /* features supported */
    char* localize;            /* desired language tags */
};

/* structure containing elements for a channel close */
struct chan_close {
  struct session* session;  /* session for closing the channel */
  long channel_number;      /* channel number to close */
  struct diagnostic* diag;  /* diagnostic message and number */
  char * localize;          /* xml:lang attribute on close */
};

/* doc++ note: because the core transparently manages the encoding,
   i'm not documenting that field... */
/**
 * A structure for binding a profile-module to a channel.
 * <p>
 * Unless otherwise noted, all fields should be considered read-only by the
 * profile.
 *  
 * @field next A pointer to another profile structure.
 *	This may be used in {@link bp_start_request bp_start_request} and
 *      {@link pro_start_indication pro_start_indication}.
 *
 * @field uri The URI associated with the profile.
 *
 * @field piggyback A character pointer, or <b>NULL</b>.
 *
 * @field piggyback_length The length in octets of the piggyback.
 *
 * @see pro_start_indication
 * @see pro_start_confirmation
 * @see bp_start_request
 * @see bpc_start_response
 **/
struct profile {
  struct profile * next;
  char * uri;
  char * piggyback; 
  int  piggyback_length;
  char encoding;
};


/* * * UpperAPI frame-based calls * * */



  /* This `obtains' the next frame from the given session. 
     If channel is -1, it specifies any channel.
     Returns NULL if no such frame is ready.
     For convenience, payload[size]=='\0'
     */
struct frame * blu_frame_read(
  struct session * session,
  long channel);

  /* This allocates a frame structure with a payload
     that can hold at least size octets. The frame
     is charged against the memory_limit. 
     f->session is set from session.
     next_in_message and next_on_channel are NULL.
     msg_type and more are ' '
     channel_, message_, and answer_number are -1.
     payload points to at least size+2 octets of space.
     f->size is initialized from size, but may be reduced.
     f->payload should not be modified.
     */
struct frame * blu_frame_create(
  struct session * session,
  size_t size);

  /* This sends the frame, then deallocates it once
     all the payload octets have been written.
     The frame must have come from blu_frame_create(). */
void blu_frame_send(
  struct frame * frame);
  /* Requires session to be filled in, 
     next_in_message to be NULL or valid, 
     next_in_channel to be NULL,
     msgtype to be filled in, 
     channel_number to be filled in, 
     msgno can be -1 to mean "pick one" if more=='.',
     ansno can be -1 to mean "pick one" 
       if msgno is filled in and more=='.',
     more must be filled in,
     size can be -1 to use strlen() on payload,
     payload must be filled in and MUST point to 
       the same place as returned from blu_frame_create().
     */


  /* This `obtains' the first (or all) frames that match
     the query and returns a pointer to the first. If
     no frames match the query, it returns NULL. */
struct frame * blu_frame_query(
  struct session * session, /* Must be valid */
  long channel, /* specific channel, or -1 for any */
  char msgtype, /* ' ' for any, or 'M', 'A', ... */
  long msgno,   /* -1 for any, or a message number */
  long ansno,   /* -1 for any, or an answer number */
  char more);   /* '*' for incomplete, '.' for complete */
  /* If `more' is not '*', then the frames returned may be 
     linked via the `frame' pointer in the frame returned.
     All such linked frames are `obtained'. If `more' is '*',
     a '.' frame may be returned. */
  /* Note that if the profile waits for a complete message,
     and the message is bigger than the window, the 
     profile will deadlock. */

  /* This takes a linked list of frames linked on the 
     "next_in_message" field (as returned by blu_frame_query,
     or as constructed by the application). It returns
     a single frame allocated from the local memory pool.
     This single frame has the same attributes as the last
     frame on the linked list, but with a payload which
     is the concatenation of all the frame payloads and
     an appropriate size. The incoming frames are not
     freed in any way. This probably makes no sense for
     frames that are not all part of the same message. 
     It returns NULL if memory couldn't be allocated,
     or if "in" is NULL. Allocates the new frame from
     the provided session, regardless of where "in" is
     allocated from. */
struct frame * blu_frame_aggregate(
  struct session * sess,
  struct frame * in);


  /* This frees a frame that has been `obtained' via
     blu_frame_create or blu_frame_read or 
     blu_frame_query. If the frame was read from the 
     socket, freeing it opens up the receive
     window, possibly sending a SEQ. If the frame
     was allocated locally, it reverses the charge
     against memory_limit. */
void blu_frame_destroy(
  struct frame * frame);

  /* This returns the following status for the channel:
     0 - Channel is closed.
     1 - Channel exists but is half started.
     2 - Channel exists but is half closed.
     3 - Channel exists and is busy.
     4 - Channel exists: all queued MSGs have been fully answered.
     5 - Channel exists: No unanswered messages or queued frames.
     Note that if a frame is in the process of being received, 
     this might say it's quiescent until the "END\r\n" is received.
     */
int blu_channel_status(
  struct session * sess,
  long channel_number);

/*
 * blu_channel_statusflag_get
 * blu_channel_statusflag_set
 *
 * These are a hack because the tuning reset stuff is not in CBEEP.c yet.
 */
char blu_channel_statusflag_get(
  struct session * sess,
  long channel_number);

void blu_channel_statusflag_set(
  struct session * sess,
  long channel_number,
  char stat);


  /* This returns the count of the number of frames
     meeting the selection criteria on the input queue
     of the given channel.
     If msg_type is a space, then any frame type matches;
     otherwise, only frames of the given type match.
     If more is a period, only complete frames are counted;
     otherwise, all frames are counted.
     For example, blu_channel_frame_count(s,3,'A','.')
     returns the number of complete answers queued 
     to be read from channel 3.
     */
int blu_channel_frame_count(
  struct session * sess,
  long channel_number,
  char msg_type,
  char more);

/* * * * * * *  Upper Layer query-type calls * * * * * * * */


/* * * * * * * * User Information * * * * * * * * */

  /* Sets a pointer in the session that is
     otherwise ignored by the BEEP library. */
void blu_session_info_set(
  struct session * session,
  void * user_info);

  /* Returns the pointer set by blu_session_info_set.
     Returns NULL if never set. */
void * blu_session_info_get(
  struct session * session);

  /* Set a pointer associated with a given channel
     on a given session, otherwise unmanaged. */
void blu_channel_info_set(
  struct session * session,
  long channel,
  void * user_info);

  /* Get a pointer associated with a given channel
     on a given session, otherwise unmanaged.
     Returns NULL if never set. */
void * blu_channel_info_get(
  struct session * session,
  long channel);

/* * * * * * * Upper layer channel management calls * * * * * * * */


  /* This queues a message to start one of the 
     indicated profiles on the indicated channel.
     If channel==-1, an approrpriate channel number 
     is chosen. The channel number is returned. */
long blu_channel_start(
  struct session * session,
  struct chan0_msg * chan0);

  /* This queues a message to close the indicated channel.
     If channel==0, this closes the entire session. */
void blu_channel_close(
  struct session * session,
  long channel,
  struct diagnostic * error);

  /* This parses and returns the next incoming message on
     channel 0, if any. It returns NULL if none.
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
struct chan0_msg * blu_chan0_in(
  struct session * session);

  /* This reclaims the memory occupied by the parsed version
     of the channel zero confirmation as well as the frames
     it occupies. */
void blu_chan0_destroy(
  struct chan0_msg * msg);

  /* This replies to an indication.
     If profile!=NULL, then the channel is opened.
     If error!=NULL and profile!=NULL, the error message
       is formatted and returned as the PCDATA of the
       <profile> element returned.
       --> Huh? why not the content of struct profile?
     If error!=NULL and profile==NULL, the channel
       is not opened.
     */
void blu_chan0_reply(
  struct chan0_msg * indication,
  struct profile * profile,
  struct diagnostic * error);

  /* This initiates or cancels a tuning reset on the indicated channel.
     If "channel" is non-negative, all frames for channels *other* than
     the one indicated in "channel" are blocked. (Of course, the current
     frame is finished being sent.) If "channel" is -1, then any
     blockage is removed and all processing goes back to normal.
     It is up to the wrapper to manage timing and channel flushing 
     and such. */
void blu_tuning_reset(
  struct session * session,
  long channel);

  /* This sets the maximum amount of incoming payload that 
     will be buffered inside the library for the given channel. 
     Once a frame is obtained and disposed, a SEQ will be sent
     to allow more payload for this channel. It returns the
     previous value, which defaults to 4096. It queues a SEQ
     immediately if appropriate. A window of -1 returns the
     ole value without changing it. */
long blu_local_window_set(
  struct session * session,
  long channel,
  long window);

  /* This returns the current outgoing window size available. */
long blu_peer_window_current_get(
  struct session * session,
  long channel);

  /* This returns the largest recent outgoing window size. */
long blu_peer_window_maximum_get(
  struct session * session,
  long channel);

void bll_session_debug_print(struct session *s, FILE *f);

#if defined(__cplusplus)
}
#endif

#endif
