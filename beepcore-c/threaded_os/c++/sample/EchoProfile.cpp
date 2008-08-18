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
// EchoProfile.cpp: implementation of the EchoProfile class.
//
//////////////////////////////////////////////////////////////////////

#include "bp_wrapper.h"
#include "Channel.h"
#include "Common.h"
#include "EchoProfile.h"
#include "Message.h"
#include "Thread.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// static lock we use to guard data when we spin off threads
Mutex EchoProfile::threadStartupLock = Mutex();

const char EchoProfile::URI[] = "http://xml.resource.org/profiles/NULL/ECHO";

void *channelServiceRoutine(void *arg);
int handleMessage(Channel *ch);

EchoProfile::EchoProfile()
  :Profile((char*)URI, false), SimpleProfile((char*)URI, false)
{
}

EchoProfile::~EchoProfile()
{
}

ProfileRegistration *EchoProfile::createProfileRegistration()
{
  return new ProfileRegistration((char*)URI, this, 
				 (char*)ProfileRegistration::PLAINTEXT,
				 (char*)ProfileRegistration::PLAINTEXT, 
				 false, false);
}

void EchoProfile::channelStarted(Channel *ch)
{
  printf("Echo Profile Channel Started\n",NULL);

  // Vestigal logic, ignore it.
  // At one point, I was going to break this down so it could be both thread
  // per channel and do it's own thing.  I decided to do that in another
  // version of EchoProfile.
  //if(!thread_per_channel)
  //{
  //}
  //else
  //{
  // In ChannelStarted
  // Store a CI->Channel mapping
  //}

  threadStartupLock.lock();
  bool *b = new bool();
  
  // Prepare data for spinoff
  struct channelThreadManagementStruct *ctms = new struct channelThreadManagementStruct();
  ctms->ch = ch;
  ctms->b = b;
  
  // Spin off a thread to handle requests on this channel
  Thread *t = Thread::startThread(&channelServiceRoutine, (void*)ctms);
  if(t == NULL)
    {
      dprintf("FAILED TO START THREAD!\n", NULL);
      assert(false);
    }
  unsigned int key = (unsigned int)ch;
  
  // Store the thread according to the channel pointer so we can stop it later
  // We could also store this in the service routine if we wanted to and
  // then just place the thr_self() result in (pthread_t) or handle instead
  // of a Thread*.  Stick with the general Thread* for now.
  
  channelThreadFlags.insert(ChannelToFlagMap::value_type(key, b));
  channelThreads.insert(ChannelToThreadMap::value_type(key, t));
  threadStartupLock.unlock();
}

void EchoProfile::channelClosed(Channel *ch)
{
  // Vestigal logic - see notes in previous routine
  //if(!thread_per_channel)
  //{
  // Look up the appropriate thread object
  //}
  //else
  //{
  // In ChannelStopped
  // Clear this mapping
  //}

  printf("Echo Profile Channel Closed\n",NULL);

  Thread *t = NULL;
  bool *b = NULL;
  unsigned int key = (unsigned int)ch;
  dprintf("Currently there are %d service threads for EchoProfile\n", channelThreads.size());
  
  threadStartupLock.lock();
  ChannelToThreadMap::iterator it = channelThreads.find(key);
  if(it != channelThreads.end())
    {
      t = (Thread*)(*it).second;
      channelThreads.erase(key);
    }
  else
    {
      dprintf("Encountered an error on Echo Profile Thread management", NULL);
      assert(false);
      return;
    }
  ChannelToFlagMap::iterator ij = channelThreadFlags.find(key);
  if(ij != channelThreads.end())
    b = (bool*)(*it).second;
  else
    {
      dprintf("Encountered an error on Echo Profile Thread management", NULL);
      assert(false);
      return;
    }
  
  *b = false;
  channelThreads.erase(key);
  channelThreadFlags.erase(key);
  
  dprintf("Currently there are %d service threads for EchoProfile\n", channelThreads.size());
  
  // Stop the service routine  
  dprintf("Stopping spawned channel service thread.\n",NULL);
  ch->signal();
  Thread::stop(t);
  threadStartupLock.unlock();
  Thread::join(t);
  delete(t);
  dprintf("Exiting from EchoProfile::stopChannel\n", NULL);
}

void EchoProfile::handle_listening_message_arrival(CHANNEL_INSTANCE *ci)
{
  Channel *ch = NULL;
  
  // Here, look the CI up, get hte channel, and call handle message on it.
  if(thread_per_channel)
    handleMessage(ch);
}

void *channelServiceRoutine(void *arg)
{
  dprintf("Spawned channel service routine for Echo Profile %d\n",NULL);

  struct channelThreadManagementStruct *ctms = (struct channelThreadManagementStruct*)arg;
  Channel *ch = (Channel*)ctms->ch;
  bool *running = (bool*)ctms->b;
  long chNum = ch->getChannelNumber();
  
  dprintf("Waiting for a message on channel on channel %d\n",chNum);      
  while(running)
    {
      if(handleMessage(ch) == 0)
	running = false;
    }
  delete(running);
  delete(ctms);
  dprintf("\n\n\n\n EXITING ***!!!\n\n\n\n", NULL);
}

int handleMessage(Channel *ch)
{
  Message *msg = NULL, *rpy = NULL;
  char *buffer = NULL;
  long chNum = ch->getChannelNumber(), length = 0;

  while(true)
    {
      msg = ch->receiveMessage(10);
      
      if(msg != NULL)
	{
	  length = msg->getSize();
	  
	  buffer = new char[length+1];
	  msg->read(buffer, 0, length);
	  printf("EchoProfile received a message.  Now echoing it back.\n",buffer);
	  dprintf("EchoProfile echoing %s\n",buffer);
	  dprintf("Deleting MSG\n",NULL);
	  delete(msg);
	  
	  buffer[length]=0;
	  rpy = new Message(buffer, length, ch, Frame::REPLY_TYPE,
			    msg->getMessageNumber());
	  dprintf("Sending reply\n",NULL);
	  tool();
	  ch->sendReply(rpy);
	  delete[] buffer;
	  dprintf("Deleting RPY\n",NULL);
	  delete(rpy);
	  msg = NULL;
	  rpy = NULL;
	  buffer = NULL;
	  return 1;
	}
      else
	{
	  return 0;
	}
    }
}


// These callbacks are provided to the application so that
// it can manage its references to channels and sessions
// appropriately.  The application is responsible for deleting
// these objects, as it may keep references to them around that
// the library cannot control.
void EchoProfile::handle_listening_channel_closure(Channel *c)
{
  dprintf("HANDLING LISTENING CHANNEL KILL %x\n", c);
  delete(c);
}

void EchoProfile::handle_listening_session_closure(Session *s)
{
  dprintf("HANDLING LISTENING SESSION KILL %x\n", s);
  delete(s);
}

