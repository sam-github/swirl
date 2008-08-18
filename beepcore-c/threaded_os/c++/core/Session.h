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
// Session.h: interface for the Session class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SESSION_H__B0CAEBCC_F508_4828_A307_B26BA094C1E1__INCLUDED_)
#define AFX_SESSION_H__B0CAEBCC_F508_4828_A307_B26BA094C1E1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Peer;
#include "SessionContext.h"
#include "Channel.h"
#include "Mutex.h"
#include "Peer.h"
#include "CloseChannelException.h"
#include "StartChannelException.h"
#include "BEEPException.h"
#include "bp_wrapper.h"

#ifdef WIN32
#pragma warning(disable:4786)
#include <windows.h>
#endif

struct channelZeroResults
{
  Session *session;
  struct chan0_msg *msg;
  struct chan0_msg *rpy;
  CHANNEL_INSTANCE *ci;
  DIAGNOSTIC * diag;
  Condition *cond;
  bool complete;
  char ch;
};

#include <list>
#include <map>
using namespace std;
typedef map<long, Channel*> ChannelTable;
typedef map<long, struct channelZeroResults *> PendingMessagesMap;
typedef list<long> PendingChannelOperationList;

class Channel;
class Frame;
#include "Common.h"

class Session
{
  friend class Channel;
  friend class Frame;
  friend class Profile;
 public:
  Session(SessionContext *sc, Peer *p, PROFILE_REGISTRATION *list,
	  long nextChannel, char *identifier);
  virtual ~Session();
  virtual char *getIdentifier(){return id;}
  WRAPPER *getWrapper(){return context->getWrapperStruct();}

  Channel *startChannel(char *uri, char *data = NULL, unsigned int length = 0) 
    throw (StartChannelException);
  void closeChannel(long number, char *data = NULL, unsigned int length = 0) 
    throw (CloseChannelException);
  void closeChannel(Channel *ch, char *data = NULL, unsigned int length = 0) 
    throw (CloseChannelException);

  Channel *getChannel(long number)
    throw (BEEPException);

  Channel *getChannel(char *uri)
    throw (BEEPException);

  bool isListener(){return ((nextChannelNumber%2)!=0);}

  protected:
  void receiveFrame(Frame *f); 
  struct session *getSessionStruct(){return wrapper->session;}

  void sendFrame(Channel *ch, Frame *f);

  static void handleStartCallback(CHANNEL_INSTANCE *ci, struct chan0_msg *msg, 
				  struct chan0_msg *rpy, char c, void *data);
  static void handleCloseCallback(CHANNEL_INSTANCE *ci, struct chan0_msg *msg,
				  DIAGNOSTIC *d, char c, void *data);
  Channel *addListeningChannel(PROFILE_INSTANCE *pi);
  Channel *closeListeningChannel(PROFILE_INSTANCE *pi);
 public:
  long getChannelCount();
  void close();
  static int STATUS_OPEN, STATUS_CLOSED;

 private:
  Mutex lock;
  SessionContext *context;
  ChannelTable channels;
  char *id;
  long nextChannelNumber;
  PROFILE_REGISTRATION *profileList;

  WRAPPER *wrapper;
  PendingChannelOperationList pccl, pcsl;
  int status;
  Peer *parent;

  // Something we use to differentiate between start/close operations
  // we initiate and those we receive
  static PendingMessagesMap czm;

  // Helper routines
  void deregisterProfiles(WRAPPER *w, PROFILE_REGISTRATION *reg);
  void registerProfiles(WRAPPER *w, PROFILE_REGISTRATION *reg);
  };
  
#endif // !defined(AFX_SESSION_H__B0CAEBCC_F508_4828_A307_B26BA094C1E1__INCLUDED_)
  
