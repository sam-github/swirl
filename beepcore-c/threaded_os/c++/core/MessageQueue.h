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
// MessageQueue.h: interface for the MessageQueue class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MESSAGEQUEUE_H__E81A9BC1_FF12_4DFD_AF4B_3EB64E55740E__INCLUDED_)
#define AFX_MESSAGEQUEUE_H__E81A9BC1_FF12_4DFD_AF4B_3EB64E55740E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Channel;
#include "DataListener.h"
#include "Frame.h"
#include "Message.h"
#include "Mutex.h"

#ifdef WIN32
#pragma warning(disable:4786)
#include <windows.h>
#endif

#include <deque>
using namespace std;
typedef deque<Message *>MessageStack;


class MessageQueue : public DataListener
{
  friend class Channel;
public:
  MessageQueue();
  virtual ~MessageQueue();
  
  virtual void receiveFrame(Frame *f, Channel *ch);
  virtual Message *getNextMessage(unsigned int millis);
  virtual Message *getNextMessage();
  
 protected:
  virtual void wakeup();
 private:
  bool hasBeenNotified, aborting;
  Mutex messageLock;
  Condition messageCondition;
  static char AVAIL_ID[];
  // Queue of Messages
  MessageStack messages;
};

#endif // !defined(AFX_MESSAGEQUEUE_H__E81A9BC1_FF12_4DFD_AF4B_3EB64E55740E__INCLUDED_)
