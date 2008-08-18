/*
 * Copyright (c) 2001 Invisible Worlds, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the Blocks Public License (the
 * "License"); You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at http://www.invisible.net/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 */
// SimpleProfile.cpp: implementation of the SimpleProfile class.
//
//////////////////////////////////////////////////////////////////////


#include "SimpleProfile.h"
#include "CBEEP.h"
#include "CBEEPint.h"
#include "Channel.h"
#include "bp_wrapper.h"
#include "Session.h"
#include "Thread.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//extern void find_best_seq_to_send(struct session *);
extern long blu_peer_current_window_get(struct session *s, long channel);
extern long blu_peer_maximum_window_get(struct session *s, long channel);

SimpleProfile::SimpleProfile(char *u, int threadPerChannel)
  :Profile(u, threadPerChannel)
{
  // Superclass does most of 'it'.
  //dprintf("SIMPLEPROFILE WAS AS FOLLOWS...%x\n", this);
}

SimpleProfile::~SimpleProfile()
{
}

char *SimpleProfile::handle_connection_init(PROFILE_REGISTRATION *pr, WRAPPER *w)
{
  //dprintf("SimpleProfile handling process_init\n", NULL);
  return NULL;
}

char *SimpleProfile::handle_connection_fin(PROFILE_REGISTRATION *pr, WRAPPER *w)
{
  //dprintf("SimpleProfile handling process_fin\n", NULL);
  return NULL;
}

char *SimpleProfile::handle_session_init(PROFILE_REGISTRATION *pr, WRAPPER *w)
{
  //dprintf("SimpleProfile handling session_init wrapper %x\n", w);
  return NULL;
}

char *SimpleProfile::handle_session_fin(PROFILE_REGISTRATION *pr, WRAPPER *w)
{
  //dprintf("SimpleProfile handling session_fin\n", NULL);
  return NULL;
}

struct chan0_msg *SimpleProfile::handle_start_indication(PROFILE_INSTANCE *pi, 
							 struct chan0_msg *msg)
{
  dprintf("SimpleProfile handling start_indiciation\n", NULL);

  // Approve
  Session *s = (Session*)pi->channel->wrapper->externalSessionReference;
  Channel *ch = addListeningChannel(pi,s);
  if(ch != NULL)
    {
      this->channelStarted(ch);

      // Tell the session to create a Channel and host it given what we know here...
      return blw_chan0_msg_new(pi->channel->wrapper,  msg->channel_number, 
			       msg->message_number,
			       'c', msg->op_sc, msg->profile, NULL, msg->features,
			       msg->localize, msg->server_name);
    }

  return NULL;
}

struct diagnostic *SimpleProfile::handle_close_indication(PROFILE_INSTANCE *pi, 
							  struct chan0_msg *msg, 
							  char x, char y)
{
  dprintf("SimpleProfile handling close_indication on ch#%d\n",
	 (int)pi->channel->channel_number);

  // These two steps determine if there is a channel to be closed - 
  // the Session knows if it initiated the close that this indication
  // is for.  If we did initiate it, then we get a NULL back and ignore
  // the result.
  Session *s = (Session*)pi->channel->wrapper->externalSessionReference;
  Channel *ch = closeListeningChannel(pi, s);
  if(ch != NULL)
    {
      dprintf("Sending close response\n", NULL);
      bpc_close_response(pi->channel, msg, NULL);
      this->channelClosed(ch);
    }
  return NULL;
}

void SimpleProfile::handle_start_confirmation(PROFILE_INSTANCE *pi, struct chan0_msg *msg,
					      struct chan0_msg *rpy, void *data)
{
  dprintf("SimpleProfile handling start_confirmation %x\n", msg);
  // Consider doing this here and doing what the wrapper does (?)
  // for consistency...we'd have to unpack a lot of data from the request
  //Session *s = (Session*)pi->channel->wrapper->externalSessionReference;
  //s->handleStartConfirmation(pi, msg, rpy, data);
  /*
  if (rpy->profile->piggyback_length) 
  {
    dprintf("Got payload '%s'\n\n", rpy->profile->piggyback);
  }
  */
}

void SimpleProfile::handle_close_confirmation(PROFILE_INSTANCE *pi, 
					      char c, void *arg)
{
  dprintf("SimpleProfile handling close_confirmation code=>%c\n", c);
  // Consider doing this here and doing what the wrapper does (?)
  // for consistency...we'd have to unpack a lot of data from the request
  //Session *s = (Session*)pi->channel->wrapper->externalSessionReference;
  //s->handleCloseConfirmation(pi, c);
}

void SimpleProfile::handle_tuning_reset_indication(PROFILE_INSTANCE *pi)
{
  //dprintf("SimpleProfile handling tuning_reset_indication\n", NULL);
  // Do nothing
}

void SimpleProfile::handle_tuning_reset_confirmation(PROFILE_INSTANCE *pi, int i)
{
  //dprintf("SimpleProfile handling tuning_reset_confirmation\n", NULL);
  // Do the tuning reset I guess...
}

void SimpleProfile::handle_tuning_reset_handshake(PROFILE_INSTANCE *pi)
{
  //dprintf("SimpleProfile handling tuning_reset_handshake\n", NULL);
  // Do the tuning reset I guess...
}

void SimpleProfile::handle_frame_available(PROFILE_INSTANCE *pi)
{
  dprintf("SimpleProfile handling frame_available\n", NULL);
  // Get the frame
  CHANNEL_INSTANCE *channel = pi->channel;
  struct frame *f = NULL;

  // TODO Do some stuff on type
  dprintf("Querying the wrapper for the frame...%x\n", channel);
  while(f == NULL)
    {
      f = bpc_query_message(channel, BLU_QUERY_ANYTYPE,
			     BLU_QUERY_ANYMSG, BLU_QUERY_ANYANS);
      if(f == NULL)
	{
	  dprintf("Unable to fetch frame as advertised.  ERROR!!!\n", NULL);
	  Thread::yield();
	}
    }
  dprintf("Queried and fetched frame.\n", NULL);
  /*
    if(f == NULL)
    f = bpc_query_message(channel, BLU_QUERY_ANYTYPE,
			 BLU_QUERY_ANYMSG, BLU_QUERY_ANYANS);
    {
      dprintf("We're messed, null frame back from query\n", NULL);
      return;
    }
  */


  // Place the Frame on the Q to be processed, or rather, get our associated
  // channel and add the frame to it.
  Session *s = (Session*)channel->wrapper->externalSessionReference;
  FRAME *temp = NULL;
  while(f != NULL)
    {
      Frame *frame = new Frame(f, channel);
      assert(frame != NULL);
      postFrame(frame, s);
      temp = f;
      f = f->next_in_message;
      temp->next_in_message = NULL;
    }
}

void SimpleProfile::handle_message_available(PROFILE_INSTANCE *pi)
{
  //dprintf("SimpleProfile handling message_available (IGNORING)\n", NULL);
}

void SimpleProfile::handle_window_full(PROFILE_INSTANCE *pi)
{
  //dprintf("SimpleProfile handling window_full\n", NULL);

  // Not sure what to do here yet TODO
  //find_best_seq_to_send(pi->channel->wrapper->session);
}

void SimpleProfile::handle_instance_destroy(PROFILE_INSTANCE *pi)
{
  //dprintf("SimpleProfile handling instance_destroy\n", NULL);
  // Smoke our profileList entries and remove all mappings from profileMap
}
