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
// Frame.cpp: implementation of the Frame class.
//
//////////////////////////////////////////////////////////////////////
#include "Frame.h"
#include "Message.h"
#include "Session.h"

//////////////////////////////////////////////////////////////////////
// Static Constants
//////////////////////////////////////////////////////////////////////
char Frame::ANSWER_TYPE		= 'A';
char Frame::ERROR_TYPE		= 'E';
char Frame::LAST		= '.';
char Frame::MESSAGE_TYPE	= 'M';
char Frame::MORE		= '*';
char Frame::NULL_TYPE		= 'N';
char Frame::REPLY_TYPE		= 'R';

// debugging
//static int frame_count = 0;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
Frame::Frame(const char *data, unsigned int length, 
	     Channel *ch, long messageNumber, char type,
	     bool last)
    :Data(OUTBOUND), channel(ch->channelInstance)
{
  frameStruct = bpc_frame_allocate(ch->getChannelStruct(), length);
  bcopy((const void*)data, (void*)frameStruct->payload, length);
  if(last)
    frameStruct->more = LAST;
  else
    frameStruct->more = MORE;
  frameStruct->channel_number = ch->getChannelNumber();
  frameStruct->message_number = messageNumber;
  frameStruct->msg_type = type;
  frameStruct->next_in_message = NULL;
  dprintf("Frame Constructor (outbound) ch=>%d ",frameStruct->channel_number);
  dprintf("msg=>%d ", messageNumber);
  dprintf("type=%c ", type);
  dprintf("next=>%x ", frameStruct->next_in_message);
  dprintf("size %d ", frameStruct->size);
  dprintf("data=>%x\n", frameStruct->payload);
  //frame_count++;
  //dprintf("FCount=%d\n", frame_count);
}

Frame::Frame(struct frame *f, CHANNEL_INSTANCE *channel)
    :frameStruct(f), Data(INBOUND), channel(channel)
{
  dprintf("Frame Constructor (inbound) ch=>%d ",frameStruct->channel_number);
  dprintf("msg=>%d ", frameStruct->message_number);
  dprintf("type=%c ", frameStruct->msg_type);
  dprintf("next=>%x ", frameStruct->next_in_message);
  dprintf("size %d ", frameStruct->size);
  dprintf("data=>%x\n", frameStruct->payload);
  assert(frameStruct != NULL);
  //frame_count++;
  //dprintf("FCount=%d\n", frame_count);
}

Frame::~Frame()
{
  dprintf("Frame::~Frame\n",NULL);
  // The C Library cleans up frame structs we send...
  // so only delete them if it's one that the lib sent us.
  if(dataMode == INBOUND)
    {
      dprintf(" msgno=>%d\n", frameStruct->message_number);
      bpc_message_destroy(channel, frameStruct);
    }
  //frame_count--;
  //dprintf("FCount=%d\n", frame_count);
}

//////////////////////////////////////////////////////////////////////
// Data Members
//////////////////////////////////////////////////////////////////////
bool Frame::isLast()
{
	return (frameStruct->more == Frame::LAST);
}

int Frame::read(char *in_buffer, unsigned int start,
		unsigned int length)
{
	// TODO why can't I use the virtual static here?
	// dprintf("Frame Read %d %d %d\n", offset, start, length);
  return Data::read(in_buffer, start, length, frameStruct->payload,
		    offset, frameStruct->size);
}

unsigned int Frame::getDataOffset(char *data, 
				  unsigned int length)
{
	assert(length>1);

	for(int i=1; i < length; i++)
	{
		if( data[i] == Message::NEWLINE && 
			data[i-1 ] == Message::LINEFEED )
			return i+1;
	}
	return (unsigned int)-1;
}
