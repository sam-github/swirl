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
// Channel.h: interface for the Channel class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CHANNEL_H__D5CC5A60_7C81_4B4F_A3A8_BB62ADD1B2F7__INCLUDED_)
#define AFX_CHANNEL_H__D5CC5A60_7C81_4B4F_A3A8_BB62ADD1B2F7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSB_VER > 1000

class DataListener;
class Session;
#include "bp_wrapper.h"
#include "Frame.h"
#include "Message.h"
#include "MessageQueue.h"
#include "Mutex.h"
#include "Session.h"

#ifdef WIN32
#pragma warning(disable:4786)
#include <windows.h>
#endif

#include <map>
using namespace std;
typedef map<long, Message*> MsgNumToMessageMap;
typedef map<long, Message*>::iterator MsgNumToMessageMapIterator;

class Channel  
{
friend class Frame;
friend class Message;
friend class Session;
 public:
 Channel(Session *s, PROFILE_INSTANCE *pi,
	 char *uri, long number, 
	 DataListener *listener = NULL);
 virtual ~Channel();
 
 virtual DataListener *getDataListener()
   {return dataListener;}
 virtual void setDataListener(DataListener *listener)
   {dataListener = listener;}
 virtual long getChannelNumber(){return channelNumber;}
 virtual char *getUri(){return uri;}

 // Synchronous IO Calls
 virtual Message *sendMessage(Message* m);
 // Timeout
 virtual Message *receiveMessage(int millis);
 virtual void sendReply(Message *m);
 
 void signal();
 protected:

 // This one is shielded to enforce the synchronous initiate path
 virtual void receiveFrame(Frame *f);
 virtual void sendFrame(Frame *f);
 
 virtual Channel& operator<<(Frame *f);
 virtual Channel& operator<<(Message *m);
 
 virtual Channel& operator>>(Frame *f);
 virtual Channel& operator>>(Message *m);
 
 CHANNEL_INSTANCE *getChannelStruct(){return channelInstance;}
 long getNextMessageNumber();

 
 protected:
 char *uri;
 PROFILE_INSTANCE *profileInstance;
 void releaseFrame(Frame *f);
 DataListener *dataListener;
 Session *session;
 long channelNumber;
 Mutex pendingLock, numLock;
 MessageQueue messageQueue;
 MsgNumToMessageMap pendingMessages, pendingReplies;
 long nextMessageNumber;
 CHANNEL_INSTANCE *channelInstance;
};

#endif // !defined(AFX_CHANNEL_H__D5CC5A60_7C81_4B4F_A3A8_BB62ADD1B2F7__INCLUDED_)
