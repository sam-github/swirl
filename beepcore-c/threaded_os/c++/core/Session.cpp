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
// Session.cpp: implementation of the Session class.
//
// Session basically registers our Channels as profile instances
// with the underlying library, so it's functionality in this
// stub mode is pretty limited.  All it does now is bootstrap
// the channel test.
//
//////////////////////////////////////////////////////////////////////
#include "Session.h"
#include "BEEPException.h"
#include "CloseChannelException.h"
#include "StartChannelException.h"
#include "Thread.h"
#include "CBEEP.h"
#include "Peer.h"
#include "logutil.h"

PendingMessagesMap Session::czm = PendingMessagesMap();
int Session::STATUS_OPEN = 1;
int Session::STATUS_CLOSED = 0;

//static int session_count = 0;
void tool()
{}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
Session::Session(SessionContext *sc, Peer *p, 
		 PROFILE_REGISTRATION *prl, long firstChannel,
		 char *identifier)
{
  //channels.insert(ChannelTable::value_type(0, NULL));
  //dprintf("\nInitializing session, channels size = %d", channels.size());
  wrapper = sc->getWrapperStruct();
  context = sc;
  nextChannelNumber = firstChannel;
  id = identifier;

  // Maintain either a profile registry association
  // or a reference to the peer.
  profileList = prl;
  parent = p;

  // Register profiles
  registerProfiles(wrapper, prl);

  // Update status
  status = STATUS_OPEN;

  //session_count ++;
  //dprintf("\nSession::Session count=>%d\n", session_count);
}

Session::~Session()
{
  if(parent != NULL)
    parent->removeSession(this);
  else
    {
      printf("PEER reference in session is null",NULL);
      assert(parent != NULL);
    }

  deregisterProfiles(wrapper, profileList);
  
  dprintf("Deleting ID & Wrapper\n",NULL);
  if(id != NULL)
    delete(id);
  delete(context);

  blw_wrapper_destroy(wrapper);
  dprintf("Done Deleting ID & Wrapper\n",NULL);

  //session_count --;
  //dprintf("Session::~Session count=>%d\n", session_count);
}

void Session::close()
{
  lock.lock();
  if(status == STATUS_CLOSED)
   {
     lock.unlock();
     dprintf("Aborting Session::close cuz it's already done\n", NULL);
     return;
   }
  status = STATUS_CLOSED;
  lock.unlock();
    
  wrapper->status = INST_STATUS_EXIT;
  dprintf("WRAPPER STATUS =>%c", wrapper->status);
  blw_shutdown(wrapper);

  dprintf("Session::close leaving\n", NULL);
}

void Session::sendFrame(Channel *ch, Frame *frame)
{
  if(status == STATUS_CLOSED)
     return;

  CHANNEL_INSTANCE *channel = ch->getChannelStruct();
  struct frame *temp = frame->getFrameStruct();
  char x = temp->payload[temp->size];
  temp->payload[temp->size] = 0;
  dprintf("Session::SendFrame %x ", temp);
  dprintf("CH %d ", temp->channel_number);
  dprintf("MSG %d ", temp->message_number);
  dprintf("MORE %c ", temp->more);
  dprintf("SIZE %d ", temp->size);
  dprintf("DATA %x\n", temp->payload);
  temp->payload[temp->size] = x;
  bpc_send(channel, temp);
  // TODO check the status?
  return;    
}

void Session::receiveFrame(Frame *f)
  //throw (DataTransmissionException)
{
  dprintf("Session::recvFrame on ch#%d\n", f->getChannelNumber());
  long i = f->getChannelNumber();
  Channel *ch = NULL, *ch2 = NULL;
  ChannelTable::iterator it = channels.find(i);
  if(it != channels.end())
    ch2 = (Channel*)(*it).second;  
  ch = getChannel(i);
  // TODO consider this - killing the library thread is probably a BAD thing...
  // so call some library call that indicates an error, in the meantime..
  assert(ch != NULL);
  //if(ch == NULL)
  //  throw DataTransmissionException();
  ch->receiveFrame(f);
  dprintf("Session::RcvFrame forwarded received frame to channel\n", NULL);
}

Channel *Session::startChannel(char *uri, char *data,
			       unsigned int length)
  throw (StartChannelException)
{
  struct profile a_profile;
  a_profile.uri = uri;
  a_profile.next = NULL;
  a_profile.piggyback = data;
  if(data != NULL)
    length = strlen(data);
  a_profile.piggyback_length = length;
  
  long channelNumber;

  lock.lock();
  channelNumber = nextChannelNumber++;
  // Record the # in a list so we can know if we initiated this or not later
  // when we receive the indication/confirmation
  pcsl.push_back(channelNumber);
  lock.unlock();

  dprintf("Attempting to create channel #%d\n", channelNumber);
  struct chan0_msg *msg =  NULL;
  msg = blw_chan0_msg_new(wrapper, channelNumber, -1, 'i' , 's',  
			  &a_profile, NULL, "", "", "localhost");

  // Set up our callback's information
  struct channelZeroResults *sczr = new struct channelZeroResults();
  sczr->msg = msg;
  sczr->rpy = NULL;
  sczr->diag= NULL;
  sczr->ci = NULL;
  sczr->ch  = 0;
  sczr->session = this;
  sczr->complete = false;
  sczr->cond = new Condition();

  long y = (unsigned int)msg;
  czm.insert(PendingMessagesMap::value_type(y, sczr));  
  
  dprintf("Session::StartChannel sending start request and waiting for return %x\n", msg);
  DIAGNOSTIC *diag = blw_start_request(wrapper, msg, 
				       handleStartCallback, NULL);
  if(diag != NULL)
    {
      // TODO Throw Exception
      dprintf("ERROR on Channel Start %s\n", diag->message);
      // Dispose of the DIAGNOSTIC

      throw StartChannelException();
      blw_diagnostic_destroy(wrapper,diag);
      return NULL;
    }
  
  dprintf("Blocking until we get a callback on the start request\n", NULL);
  lock.lock();
  if(!sczr->complete)
    {
      sczr->cond->wait(&lock);
    }
  else
    dprintf("Aborting block\n", NULL);
  
  // Otherwise, whee!!!
  // sczr holds it all (the info from the callback)
  czm.erase(y);
  Channel *ch = new Channel(this, sczr->ci->profile, uri, channelNumber);
  dprintf("Session::startCH successfully started channel %d\n", channelNumber);
  channels.insert(ChannelTable::value_type(channelNumber, ch));
  lock.unlock();
  delete(sczr->cond);
  delete(sczr);
  //dprintf("ChannelTable has %d channels in it\n", channels.size());
  return ch;
}

// Maps over to the other closeChannel call
void Session::closeChannel(Channel *ch, char *data,
			   unsigned int length)
  throw (CloseChannelException)
{
  if(status == STATUS_CLOSED && ch->getChannelNumber() != 0)
     return;
  closeChannel(ch->getChannelNumber());
}

void Session::closeChannel(long channelNumber, 
			   char *data, unsigned int length)
  throw (CloseChannelException)
{
  if(status == STATUS_CLOSED && channelNumber != 0)
     return;
  // Validate that it's here.
  Channel *ch = getChannel(channelNumber);

  if(ch == NULL && channelNumber != 0)
    {
      // TODO Throw Exception
      dprintf("Close on non-existent channel\n", NULL);
      return;
    }

  dprintf("Session::CloseChannel Attempting to close channel #%d\n", channelNumber);

  DIAGNOSTIC diag;
  diag.code = 200;
  diag.lang = NULL;
  diag.message = NULL;

  struct chan0_msg *msg =  NULL;
  struct profile a_profile;
  if(ch != NULL)
    a_profile.uri = ch->getUri();
  a_profile.next = NULL;
  a_profile.piggyback = data;
  if(data != NULL)
    length = strlen(data);
  a_profile.piggyback_length = length;

  // Record the # in a list so we can know if we initiated this or not later
  // when we receive the indication/confirmation
  lock.lock();
  pccl.push_back(channelNumber);
  lock.unlock();

  msg = blw_chan0_msg_new(wrapper, channelNumber, 0, 'i' , 'c',  NULL,
			  &diag, "", "", "localhost");

  // Set up our callback's information
  struct channelZeroResults *sczr = new struct channelZeroResults();
  sczr->session = this;
  sczr->msg = msg;
  sczr->rpy = 0;
  sczr->diag= NULL;
  sczr->ci = NULL;
  sczr->ch  = 0;
  sczr->complete = false;
  sczr->cond = new Condition();

  long y = (long)msg;
  czm.insert(PendingMessagesMap::value_type(y, sczr));  
  
  dprintf("Session::CloseChannel sending close and waiting for return %x\n", msg);  
  blw_close_request(wrapper, msg, handleCloseCallback, NULL);

  lock.lock();
  dprintf("Blocking until we get a callback on the close request\n", NULL);
  if(!sczr->complete)
    sczr->cond->wait(&lock);
  else
    dprintf("Skipping the block\n", NULL);

  if(sczr->diag != NULL)
    {
      // TODO throw exception
      dprintf("Error on close %s\n", sczr->diag->message);
      lock.unlock();
      throw CloseChannelException();
      blw_diagnostic_destroy(wrapper,sczr->diag);
      return;
    }
  channels.erase(channelNumber);
  //channels.erase(channelNumber);
  lock.unlock();

  delete(sczr->cond);
  delete(sczr);

  dprintf("Succesfully closed Channel #%d\n", channelNumber);
  //dprintf("ChannelTable has %d channels in it", channels.size());
}

// TODO Register the 
Channel *Session::getChannel(char *uri)
  throw (BEEPException)
{
  dprintf("Get channel by uri unsupported just yet\n", NULL);
  return NULL;
}

Channel *Session::getChannel(long i)
  throw (BEEPException)
{
  ChannelTable::iterator it = channels.find(i);
  if(it == channels.end())
    return NULL;
  else
    return (Channel*)(*it).second;
}

// Called by the wrapper when the confirmation is made - so this is really
// used by the initiator alone
void Session::handleStartCallback(CHANNEL_INSTANCE *ci, struct chan0_msg *msg, 
				  struct chan0_msg *rpy, char ch, void *data)
{
  dprintf("Received callback for Start Channel Request %x\n", ci);
  // TODO - something more intelligent and wrapper-like
  assert(ci != NULL && msg != NULL && rpy != NULL);

  // Do our necessary dirty work so our 'callbacks' work
  if(ci->profile_registration == NULL)
    {
      // TODO Log this instead...Commonly happens when the
      // server disconnects midstream or something like that.
      dprintf("\n\n *** ERROR CASE - no profilereg? *** \n\n", NULL);
      return;
    }
  Profile *p = (Profile*) ci->profile_registration->externalProfileReference;
  ci->profile->user_ptr2 = p;

  // We stash this away in case we need it
  long y = (long)msg;
  PendingMessagesMap::iterator it = czm.find(y);

  if(it != czm.end())
    {
      struct channelZeroResults *s = (*it).second;
      s->complete = true;
      s->rpy = rpy;
      s->ch = ch;
      s->diag = NULL;
      s->ci = ci;
      ci->externalSessionReference = (void*)s->session;
      s->cond->notify();
      return;
    }
  dprintf("Received unsolicited Channel Zero Message!\n", NULL);
  assert(1);
}

// Called by the wrapper when the confirmation is made - so this is really
// used by the initiator alone
void Session::handleCloseCallback(CHANNEL_INSTANCE *ci, struct chan0_msg *msg,
				  DIAGNOSTIC *d, char ch, void *data)
{
  dprintf("Received callback for Close Channel Request %x\n", ci);
  dprintf("DIAG=%x\n", d);
  // TODO - something more intelligent and wrapper-like
  assert(ci != NULL && msg != NULL);
  
  // We stash this away in case we need it
  long y = (long)msg;
  
  PendingMessagesMap::iterator it = czm.find(y);
  if(it != czm.end())
    {
      struct channelZeroResults *s = (*it).second;
      s->complete = true;
      s->ch = ch;
      s->diag = d;
      ci->externalSessionReference = (void*)s->session;
      s->cond->notify();
      return;
    }
  // Why do this? TODO Was I on drugs?
  //ci->externalSessionReference = NULL;
  
  // TODO Terminate
  dprintf("Received unsolicited Channel Zero Message! on CH %d\n", y);
  assert(1);
}

// Called by the Profile when we get an indication...
// Initiator returns, listener processes away
Channel *Session::addListeningChannel(PROFILE_INSTANCE *pi)
{
  long chnum = pi->channel->channel_number;

  // If we're initiator, then ignore this
  // If we're initiator, then ignore this
  PendingChannelOperationList::iterator it = NULL;
  for(it = pcsl.begin(); it != pcsl.end(); it ++)
    {
      if(chnum == *it)
	{
	  lock.lock();
	  pcsl.remove(chnum);
	  dprintf("Ignoring start indication that we initiated\n", NULL);
	  lock.unlock();
	  return NULL;
	}
    }

  dprintf("Adding listening channel %d ", chnum);
  dprintf("for profile %s\n", pi->uri);
  Channel *ch = new Channel(this, pi, pi->uri, chnum);
  channels.insert(ChannelTable::value_type(chnum, ch));  
  dprintf("ChannelTable now has %d channels in it\n", channels.size());
  return ch;
}

// Called by the Profile when we get an indication...
// Initiator returns, listener processes away
Channel *Session::closeListeningChannel(PROFILE_INSTANCE *pi)
{
  long chnum = pi->channel->channel_number;

  // If we're initiator, then ignore this
  PendingChannelOperationList::iterator it = NULL;
  for(it = pccl.begin(); it != pccl.end(); it ++)
    {
      if(chnum == *it)
	{
	  lock.lock();
	  pccl.remove(chnum);
	  dprintf("Ignoring close indication that we initiated\n", NULL);
	  lock.unlock();
	  return NULL;
	}
    }

  // Otherwise, we are a listener, so go for it.
  dprintf("Removing listening channel %d ", chnum);
  dprintf("for profile %s\n", pi->uri);
  Channel *ch = this->getChannel(chnum);
  channels.erase(chnum);
  dprintf("ChannelTable now has %d channels in it\n", channels.size());
  return ch;
}

long Session::getChannelCount()
{
  return channels.size();
}

void Session::registerProfiles(WRAPPER *w, PROFILE_REGISTRATION *reg)
{
  while (reg)
    {
      DIAGNOSTIC *diag = blw_profile_register(wrapper,reg);
      if (diag)
	{
	  wrapper->log(LOG_WRAP,0,diag->message);
	  if(diag != NULL)	
	    {
	      if(diag->message != NULL)
		dprintf("Session::registerProfiles issue %s\n", diag->message);
	      else
		dprintf("Session::registerProfiles diagmessage was null\n", NULL);
	    }
	  blw_diagnostic_destroy(wrapper,diag);
	  diag=NULL;
	}
      reg=reg->next;
    }
}

void Session::deregisterProfiles(WRAPPER *w, PROFILE_REGISTRATION *reg)
{
  dprintf("Session::deregisterProfiles\n", NULL);
  blw_profile_registration_chain_destroy(w, reg);
}

