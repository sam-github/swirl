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
// EchoProfile.h: interface for the EchoProfile class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ECHOPROFILE_H__D8B9537C_7214_4700_861E_03E481D3081C__INCLUDED_)
#define AFX_ECHOPROFILE_H__D8B9537C_7214_4700_861E_03E481D3081C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Channel;
#include "bp_wrapper.h"
#include "SimpleProfile.h"
#include "ProfileRegistration.h"
#include "Thread.h"
#include "Mutex.h"

#include <map>
using namespace std;
typedef map<unsigned int, bool *> ChannelToFlagMap;
typedef map<unsigned int, Thread *> ChannelToThreadMap;

struct channelThreadManagementStruct
{
  Channel *ch;
  bool *b;
};

class EchoProfile : public virtual SimpleProfile  
{
 public:
  EchoProfile();
  virtual ~EchoProfile();

  virtual char *getUri(){return (char*)URI;}
  virtual ProfileRegistration *createProfileRegistration();

  virtual void channelStarted(Channel *ch);
  virtual void channelClosed(Channel *ch);

  virtual void handle_listening_message_arrival(CHANNEL_INSTANCE *ci);
  virtual void handle_listening_channel_closure(Channel *c);
  virtual void handle_listening_session_closure(Session *s);


 protected:
  //  virtual void handle_frame_available(PROFILE_INSTANCE *);
  static Mutex threadStartupLock;

 private:
  static const char URI[];
  ChannelToFlagMap channelThreadFlags;
  ChannelToThreadMap channelThreads;
};

#endif // !defined(AFX_ECHOPROFILE_H__D8B9537C_7214_4700_861E_03E481D3081C__INCLUDED_)
