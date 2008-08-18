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
// Channel.cpp: implementation of the Channel class.
//
// Channel is kind of a place holder, somewhere we can keep a reference
// to a data listener, aggregate messages, and manage IO conditions.
// The real IMPL of Channel is in the CBEEPLib and the CWrapper.
//////////////////////////////////////////////////////////////////////

#include "Channel.h"
#include "Session.h"
#include "DataTransmissionException.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
// Used for testing
#ifdef WIN32
DWORD WINAPI test_listener(void *arg);
static bool done = false;
#endif

static int channel_count = 0;

Channel::Channel(Session *s, PROFILE_INSTANCE *pi,
		 char *u, long number, 
		 DataListener *listener)
  :channelNumber(number), dataListener(listener),
   nextMessageNumber(0)
{
  uri = u;
  profileInstance = pi;
  channelInstance = pi->channel;
  session = s;
  channel_count++;
  dprintf("Channel::Channel count=>%d\n", channel_count);
}

Channel::~Channel()
{
  channel_count--;
  dprintf("Channel::~Channel count=>%d\n", channel_count);
}

// Called by session only
// Watch closely, this is about as freaky as the logic gets
void Channel::receiveFrame(Frame *f)
{
  unsigned int msgno = f->getMessageNumber();
  dprintf("CH::recvFrame %d got a frame ", channelNumber);
  dprintf("for msgno%d\n", msgno);
  pendingLock.lock();
  // Path 2, reception of a message as a listener
  if(f->getMessageType() == Frame::MESSAGE_TYPE)
    {
      messageQueue.receiveFrame(f, this);
    } // End MSG Block
  // Path 4, reception of a reply as an initiator
  // The goal is now to wake up any blocked threads
  // and cleanup the tables of pending messages here and
  // there.
  else
    {
      long msgno = f->getMessageNumber();
      Message *msg = NULL, *reply = NULL;
      MsgNumToMessageMapIterator mi = NULL;
      
      // First, have we gotten frames for this reply already?
      mi = pendingReplies.find(msgno);
      if(mi == pendingReplies.end())
	{
	  // If it wasn't in the table, then this is the first
	  // frame in the reply, so create it and stick it in the table
	  reply = new Message(f, this, msgno);
	  pendingReplies[msgno] = reply;
	  
	  // We should have a MSG waiting on this
	  mi = pendingMessages.find(msgno);
	  // TODO something less drastic IRL
	  assert(mi != pendingMessages.end());
	  msg = (*mi).second;
	  
	  // Now for the trick, put the reply where the
	  // message used to be (so we don't have to purge it)
	  // and then notify
	  pendingMessages[msgno] = reply;
	  pendingLock.unlock();
	  msg->notifyReply();		
	} // End First Frame of Reply Block
      else
	{
	  // This is not the first frame in the reply
	  // add the frame so reads can access it
	  reply = (*mi).second;
	  reply->addFrame(f);
	  
	  // If it's the last frame, remove it from our
	  // list of pending replies
	  if(f->isLast())
	    {
	      pendingReplies.erase(msgno);
	    }
	}  // End Non-First-Frame of Reply Block
    }  // End Reply Block
  pendingLock.unlock();
}

Message *Channel::sendMessage(Message *message)
{
 // Send it and block for the reply on the MessageQueue
  this->operator<<(message);

  // Put this in our map
  // TODO some weirdo enforcement...Although we'd already have
  // barfed below if the message was in error
  long i = message->getMessageNumber();
  
  dprintf("CH::SendMSG blocking, waiting for reply on MSG %d\n",i);
  pendingLock.lock();
  pendingMessages[i] = message;
  message->waitForReply(&pendingLock);
  dprintf("CH::SendMSG received reply to MSG %d, continuing\n", i);
  // No longer necessary pendingLock.unlock();
  
  //	pendingLock.lock();
  // The value in the map gets replaced underneath the sheets
  // by the same action that notifies us here, cool eh?
  Message *reply = pendingMessages[i];
  //  BOBO2
  //	pendingMessages.erase(i);
  //	pendingLock.unlock();
  return reply;
}

void Channel::sendReply(Message *m)
{
  if(m->getMessageType() == Frame::MESSAGE_TYPE)
    throw DataTransmissionException();
  this->operator<<(m);
}

Message *Channel::receiveMessage(int millis)
{
  dprintf("CH::RcvMSG\n", NULL);
  return messageQueue.getNextMessage(millis);
}

// TODO Collapse the unnecessary calls here since they get used a lot
void Channel::sendFrame(Frame *f)
{
  // Send the frame via our Session 
  session->sendFrame(this, f);
}

Channel& Channel::operator<<(Frame *f)
{
  // Send a frame
  sendFrame(f);
  return *this;
}

Channel& Channel::operator<<(Message *m)
{
  // Send a message
  Frame *f = NULL;
  while((f = m->getNextOutboundFrame()) != NULL)
    {
      sendFrame(f);
      delete(f);
    }
  return *this;
}

// TBD - Not sure we need to implement these from the
// standpoint of this library.
Channel& Channel::operator>>(Frame *f)
{
	return *this;
}

Channel& Channel::operator>>(Message *m)
{
	return *this;
}


void Channel::releaseFrame(Frame *f)
{
	// BRIDGE
	// Release an CBEEPLIB resources

	// Delete the frame
	delete(f);
}

long Channel::getNextMessageNumber()
{
  long result = 0;
  numLock.lock();
  result = nextMessageNumber++;
  numLock.unlock();
  return result;
}


void Channel::signal()
{
  messageQueue.wakeup(); 
}
