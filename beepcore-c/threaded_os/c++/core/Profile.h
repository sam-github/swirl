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
// Profile.h: interface for the Profile class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROFILE_H__F15FFCB2_488C_4EAB_BDF3_BAAF3EBEEDB3__INCLUDED_)
#define AFX_PROFILE_H__F15FFCB2_488C_4EAB_BDF3_BAAF3EBEEDB3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "ProfileRegistration.h"

class Channel;
class Profile;

#include <list>
#include <map>
using namespace std;
typedef map<PROFILE_REGISTRATION *, Profile *>ProfileMap;
typedef list<PROFILE_REGISTRATION *>ProfileList;

class Profile  
{
 public:
  Profile(char *uri, int threadPerChannel);
  virtual ~Profile();

  // Accessors
  virtual char *getUri() = 0;
  virtual ProfileRegistration *createProfileRegistration() = 0;
  
  // Callback Management 
  virtual void fillInCallbacks(PROFILE_REGISTRATION *pr);

 private:
  // Static 'C'-style callbacks
  static char *proreg_connection_init(PROFILE_REGISTRATION *pr, WRAPPER *w);
  static char *proreg_connection_fin(PROFILE_REGISTRATION *pr, WRAPPER *w);
  static char *proreg_session_init(PROFILE_REGISTRATION *pr, WRAPPER *w);
  static char *proreg_session_fin(PROFILE_REGISTRATION *pr, WRAPPER *w);

  static struct chan0_msg *proreg_start_indication(PROFILE_INSTANCE *, 
						   struct chan0_msg *);
  static struct diagnostic *proreg_close_indication(PROFILE_INSTANCE *, 
						    struct chan0_msg *, 
						    char, char);
  static void proreg_start_confirmation(PROFILE_INSTANCE *, struct chan0_msg *, 
					struct chan0_msg *, void * client_data);
  static void proreg_close_confirmation(PROFILE_INSTANCE *, char, void *);

  static void proreg_tuning_reset_indication(PROFILE_INSTANCE *);
  static void proreg_tuning_reset_confirmation(PROFILE_INSTANCE *, int);
  static void proreg_tuning_reset_handshake(PROFILE_INSTANCE *);

  static void proreg_frame_available(PROFILE_INSTANCE *);
  static void proreg_message_available(PROFILE_INSTANCE *);
  static void proreg_window_full(PROFILE_INSTANCE *);

 protected:
  // Extraction Routines
  static Profile *retrieveProfile(PROFILE_INSTANCE *pi);
  static Profile *retrieveProfile(PROFILE_REGISTRATION *pr);
  static void storeProfile(PROFILE_REGISTRATION *pr, Profile *p);

  // Wrapper Callbacks (designed to be over-written and called by the
  // static routines listed above
  virtual char *handle_connection_init(PROFILE_REGISTRATION *pr, WRAPPER *w) = 0;
  virtual char *handle_connection_fin(PROFILE_REGISTRATION *pr, WRAPPER *w) = 0;
  virtual char *handle_session_init(PROFILE_REGISTRATION *pr, WRAPPER *w) = 0;
  virtual char *handle_session_fin(PROFILE_REGISTRATION *pr, WRAPPER *w) = 0;

  virtual struct chan0_msg *handle_start_indication(PROFILE_INSTANCE *, struct chan0_msg *) = 0;
  virtual struct diagnostic *handle_close_indication(PROFILE_INSTANCE *, struct chan0_msg *, char, char) = 0;
  virtual void handle_start_confirmation(PROFILE_INSTANCE *, struct chan0_msg *, 
					  struct chan0_msg *, void * client_data) = 0;
  virtual void handle_close_confirmation(PROFILE_INSTANCE *, char, void *) = 0;

  virtual void handle_tuning_reset_indication(PROFILE_INSTANCE *) = 0;
  virtual void handle_tuning_reset_confirmation(PROFILE_INSTANCE *, int) = 0;
  virtual void handle_tuning_reset_handshake(PROFILE_INSTANCE *) = 0;

  virtual void handle_frame_available(PROFILE_INSTANCE *) = 0;
  virtual void handle_message_available(PROFILE_INSTANCE *) = 0;
  virtual void handle_window_full(PROFILE_INSTANCE *) = 0;

  virtual void handle_instance_destroy(PROFILE_INSTANCE *) = 0;

  virtual void handle_listening_channel_closure(Channel *c) = 0;
  virtual void handle_listening_session_closure(Session *s) = 0;

  // Profile Configuration Data, either at construction time, or read from 
  // ProfileConfiguration
  char *uri, *initiatorModes, *listenerModes;
  bool fullMessages, threadPerChannel;
 protected:
  void postFrame(Frame *f, Session *s);
  Channel *addListeningChannel(PROFILE_INSTANCE *pi, Session *s);
  Channel *closeListeningChannel(PROFILE_INSTANCE *pi, Session *s);
  // Flag indicating whether or not we'll use the C wrapper to provide thread per channel 
  // callback semantics.  This is set in the constructor.  A value of false indicates that
  // the developer/user/profile wishes to do its own threading.  YOU SHOULD NOT BLOCK
  // LIBRARY THREADS!
  int thread_per_channel;

 private:
  // this repository allows us to map from the C structure instance 
  // world to the C++ object instance world
  static const char PROFILE_PREFIX[], PROFILE_SUFFIX[];
  
  // This repository is per instance, and allows Profile isntances to be used with N registrations
  // as in multiple sessions with different peers/wrappers etc.
  ProfileList profileList;

};
#endif // !defined(AFX_PROFILE_H__F15FFCB2_488C_4EAB_BDF3_BAAF3EBEEDB3__INCLUDED_)
