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
// SimpleProfile.h: interface for the SimpleProfile class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SIMPLEPROFILE_H__CC9AE394_55D9_48A9_B40F_290A5912696A__INCLUDED_)
#define AFX_SIMPLEPROFILE_H__CC9AE394_55D9_48A9_B40F_290A5912696A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Channel;
class Frame;
class Message;
#include "Profile.h"

class SimpleProfile : public virtual Profile  
{
 public:
  SimpleProfile(char *uri, int threadPerChannel = true);
  virtual ~SimpleProfile();

  virtual char *getUri(){return uri;}
  virtual ProfileRegistration *createProfileRegistration() = 0;

  // "The Simple Profile API"
  virtual void channelStarted(Channel *ch) = 0;
  virtual void channelClosed(Channel *ch) = 0;

 protected:
  virtual char *handle_connection_init(PROFILE_REGISTRATION *pr, WRAPPER *w);
  virtual char *handle_connection_fin(PROFILE_REGISTRATION *pr, WRAPPER *w);
  virtual char *handle_session_init(PROFILE_REGISTRATION *pr, WRAPPER *w);
  virtual char *handle_session_fin(PROFILE_REGISTRATION *pr, WRAPPER *w);
  virtual struct chan0_msg *handle_start_indication(PROFILE_INSTANCE *, struct chan0_msg *);
  virtual struct diagnostic *handle_close_indication(PROFILE_INSTANCE *, struct chan0_msg *, char, char);
  virtual void handle_start_confirmation(PROFILE_INSTANCE *, struct chan0_msg *,
					 struct chan0_msg *, void *);
  virtual void handle_close_confirmation(PROFILE_INSTANCE *, char, void *);
  virtual void handle_tuning_reset_indication(PROFILE_INSTANCE *);
  virtual void handle_tuning_reset_confirmation(PROFILE_INSTANCE *, int);
  virtual void handle_tuning_reset_handshake(PROFILE_INSTANCE *);
  virtual void handle_frame_available(PROFILE_INSTANCE *);
  virtual void handle_message_available(PROFILE_INSTANCE *);
  virtual void handle_window_full(PROFILE_INSTANCE *);
  virtual void handle_instance_destroy(PROFILE_INSTANCE *); 
  virtual void handle_listening_channel_closure(Channel *c) = 0;
  virtual void handle_listening_session_closure(Session *s) = 0;
private:
};

#endif // !defined(AFX_SIMPLEPROFILE_H__CC9AE394_55D9_48A9_B40F_290A5912696A__INCLUDED_)
