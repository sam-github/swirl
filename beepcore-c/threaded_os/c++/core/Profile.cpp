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
// Profile.cpp: implementation of the Profile class.
//
//////////////////////////////////////////////////////////////////////

#pragma warning( disable:4786 )

#include "bp_wrapper.h"
#include "Session.h"
#include "Profile.h"
#include "Common.h"
#include "Thread.h"

const char Profile::PROFILE_PREFIX[] = "<profile uri=\'";
const char Profile::PROFILE_SUFFIX[] = "<\'/>";

///////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Profile::Profile(char *u, int threadPerChannel)
  :uri(u), initiatorModes(NULL), listenerModes(NULL), 
  thread_per_channel(threadPerChannel)
{
  // TODO Read ProfileConfiguration and adjust settings
  //dprintf("PROFILE WAS AS FOLLOWS...%x\n", this);
}

Profile::~Profile()
{
  // Go through our other data structure and find each PROFILE_REGISTRATION
  // this Profile instance is associated with
  // Remove each from profileList and delete each 
  // TODO - make sure I should be killing these things, I seem to remember they get copied..
  PROFILE_REGISTRATION *pr = NULL;
  while(!profileList.empty())
    {
      pr = profileList.front();
      profileList.pop_front();
      delete(pr);
    }
}

// Set up everything but the 'next', which the ProfileRegistry manages,
// so we's dont haves ta make no worriez aboutz it.
void Profile::fillInCallbacks(PROFILE_REGISTRATION *pr)
{
  // ProfileRegistration has filled in other fields
  pr->thread_per_channel = thread_per_channel;
  pr->next = NULL;
  pr->proreg_connection_init = proreg_connection_init;
  pr->proreg_connection_fin  = proreg_connection_fin;
  pr->proreg_session_init = proreg_session_init;
  pr->proreg_session_fin  = proreg_session_fin;
  pr->proreg_start_indication = proreg_start_indication;
  pr->proreg_start_confirmation = proreg_start_confirmation;
  pr->proreg_close_indication   = proreg_close_indication;
  pr->proreg_close_confirmation = proreg_close_confirmation;
  pr->proreg_tuning_reset_indication = proreg_tuning_reset_indication;
  pr->proreg_tuning_reset_confirmation = proreg_tuning_reset_confirmation;
  pr->proreg_tuning_reset_handshake = proreg_tuning_reset_handshake;
  pr->proreg_frame_available = proreg_frame_available;
  pr->proreg_message_available = proreg_message_available;
  pr->proreg_window_full = proreg_window_full;
  //dprintf("Profile Reference has been filled in %x\n", this);
  pr->externalProfileReference = this;
}

// All of these callbacks attempt to use virtual methods to map from
// these static routines to actual object methods of the appropriate
// profile subclass
char *Profile::proreg_connection_init(PROFILE_REGISTRATION *pr, WRAPPER *w)
{
  return (retrieveProfile(pr))->handle_connection_init(pr, w);
}

char *Profile::proreg_connection_fin(PROFILE_REGISTRATION *pr, WRAPPER *w)
{
  return (retrieveProfile(pr))->handle_connection_fin(pr, w);
}

char *Profile::proreg_session_init(PROFILE_REGISTRATION *pr, WRAPPER *w)
{
  return (retrieveProfile(pr))->handle_session_init(pr, w);
}

char *Profile::proreg_session_fin(PROFILE_REGISTRATION *pr, WRAPPER *w)
{
  dprintf("\n\n\n PROREG_SESSION_FIN \n\n\n", NULL);
  char *result = (retrieveProfile(pr))->handle_session_fin(pr, w);
  Session *s = (Session*)w->externalSessionReference;
  if(s->isListener())
    {
      dprintf("Calling %s profile to manage listening session closure",
	      s->getIdentifier());
      (retrieveProfile(pr))->handle_listening_session_closure(s);
    }
  return result;
}

struct chan0_msg *Profile::proreg_start_indication(PROFILE_INSTANCE *pi, 
						   struct chan0_msg *msg)
{
  //pi->user_ptr2 = pi->channel->profile_registration->externalProfileReference;
  return (retrieveProfile(pi))->handle_start_indication(pi, msg);
}

struct diagnostic *Profile::proreg_close_indication(PROFILE_INSTANCE *pi, 
						    struct chan0_msg *msg, 
						    char x, char y)
{
  dprintf("*** Profile PROREG_CLOSE_INDIC ***\n", NULL);
  return (retrieveProfile(pi))->handle_close_indication(pi, msg, x, y);
}

void Profile::proreg_start_confirmation(PROFILE_INSTANCE *pi, struct chan0_msg *msg, 
					struct chan0_msg *rpy, void *clientData)
{
  (retrieveProfile(pi))->handle_start_confirmation(pi, msg, rpy, clientData);
}

void Profile::proreg_close_confirmation(PROFILE_INSTANCE *pi, char c,
					void *arg)
{
  //dprintf("*** Profile PROREG_CLOSE_CONFIRM ***\n", NULL);
  (retrieveProfile(pi))->handle_close_confirmation(pi, c, arg);
  Session *s = (Session*)pi->channel->wrapper->externalSessionReference;
  if(s->isListener())
    {
      Channel *ch = s->getChannel(pi->channel->channel_number);
      dprintf("Calling %s profile to manage listening channel closure",
	      pi->channel->profile_registration->uri);
      (retrieveProfile(pi))->handle_listening_channel_closure(ch);
    }
}

void Profile::proreg_tuning_reset_indication(PROFILE_INSTANCE *pi)
{
  (retrieveProfile(pi))->handle_tuning_reset_indication(pi);
}

void Profile::proreg_tuning_reset_confirmation(PROFILE_INSTANCE *pi, int i)
{
  (retrieveProfile(pi))->handle_tuning_reset_confirmation(pi, i);
}

void Profile::proreg_tuning_reset_handshake(PROFILE_INSTANCE *pi)
{
  (retrieveProfile(pi))->handle_tuning_reset_handshake(pi);
}

void Profile::proreg_frame_available(PROFILE_INSTANCE *pi)
{
  (retrieveProfile(pi))->handle_frame_available(pi);
}

void Profile::proreg_message_available(PROFILE_INSTANCE *pi)
{
  (retrieveProfile(pi))->handle_message_available(pi);
}


void Profile::proreg_window_full(PROFILE_INSTANCE *pi)
{
  (retrieveProfile(pi))->handle_window_full(pi);
}

// Three helper routines that allow us to make upcalls into Session
// from base Profile class, in a consistent and ensurable way.
void Profile::postFrame(Frame *f, Session *s)
{
  s->receiveFrame(f);
}

Channel *Profile::addListeningChannel(PROFILE_INSTANCE *pi, Session *s)
{
  return s->addListeningChannel(pi);
}

Channel *Profile::closeListeningChannel(PROFILE_INSTANCE *pi, Session *s)
{
  dprintf("Profile::closeListeningChannel\n", NULL);
  return s->closeListeningChannel(pi);
}

// Two convenience methods that unpack the structures we've received to
// give us an Object starting point.
Profile *Profile::retrieveProfile(PROFILE_REGISTRATION *pr)
{
#ifdef DEBUG
  assert(pr->externalProfileReference != NULL);
#endif
  return(Profile*)pr->externalProfileReference;
}

Profile *Profile::retrieveProfile(PROFILE_INSTANCE *pi)
{
#ifdef DEBUG
  assert(pi->channel->profile_registration->externalProfileReference != NULL);
#endif
  return(Profile*)pi->channel->profile_registration->externalProfileReference;
}
