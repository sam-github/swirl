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
// MessageQueue.cpp: implementation of the MessageQueue class.
//
// TODO consider using 2 locks for reply/message instead of 1, not
// sure it's costing us just yet, but it may in time.  OK, I did.
//
// lastRetrieved* can start at 0 cuz we're not worrying about CH0, 
// thank goodness.
//////////////////////////////////////////////////////////////////////

#include "MessageQueue.h"
#include "Thread.h"

//////////////////////////////////////////////////////////////////////
// Statics
//////////////////////////////////////////////////////////////////////
char MessageQueue::AVAIL_ID[] = "Message Queue Availability";

//////////////////////////////////////////////////////////////////////
// Testing Stuff
//////////////////////////////////////////////////////////////////////
#ifdef WIN32
DWORD WINAPI test_producer(void *arg);
#else
void *test_producer(void *arg);
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
MessageQueue::MessageQueue()
  :hasBeenNotified(false),aborting(false),
#ifdef WIN32
   messageCondition(AVAIL_ID)
#else
  messageCondition(NULL)
#endif
{}

MessageQueue::~MessageQueue()
{
  aborting = true;
  messageCondition.notify();
}

void MessageQueue::receiveFrame(Frame *f, Channel *ch)
{
  messageLock.lock();
  
  Message *temp = NULL;
  int limit = messages.size()-1;

  if(!messages.empty())
    temp = messages[limit];
  if(temp != NULL && !temp->isComplete())
    {
      temp->addFrame(f);
      if(f->isLast() && hasBeenNotified)
	{
	  messages.pop_front();
	  hasBeenNotified = false;
	}
    }
  else
    {
      temp = new Message(f, ch, f->getMessageNumber());
      messages.push_back(temp);
    } 
  messageLock.unlock();
  messageCondition.notify();
}

Message *MessageQueue::getNextMessage()
{
  // Arbitrary - TODO manage differently in conjunction
  // with more sane thread controls once they're done.
  return getNextMessage(5);
}

Message *MessageQueue::getNextMessage(unsigned int millis)
{
  //dprintf("MQ::getNextMessage %d\n",hasBeenNotified);
  Message *result = NULL;
  
  messageLock.lock();
  while(result == NULL && !aborting)
    {
      // If there's nothing there or the one we're looking for aint there,
      // then block for a while
      if(messages.empty() || hasBeenNotified)
	{
	  messageCondition.wait(&messageLock, millis);
	}
      else
	{
	  result = messages[0];
	  if(result->isComplete())
	      messages.pop_front();
	  else
	    hasBeenNotified = true;
	}
    }
  messageLock.unlock();
  //dprintf("MSGQ::getNextMSG returning a MSG\n", NULL);
  return result;
}

void MessageQueue::wakeup()
{
  aborting = true;
}
