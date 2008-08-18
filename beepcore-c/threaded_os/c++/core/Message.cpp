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
// Message.cpp: implementation of the Message class.
//
//////////////////////////////////////////////////////////////////////

#include "Message.h"
#include "Channel.h"

//////////////////////////////////////////////////////////////////////
// Statics
//////////////////////////////////////////////////////////////////////
char Message::NEWLINE = '\n';
char Message::LINEFEED = '\r';
char Message::COLON_SPACE[] = ": ";
char Message::CONTENT_TYPE[] = "Content-Type";
char Message::CONTENT_TRANSFER_ENCODING[] = "Content-Transfer-Encoding";
char Message::PAYLOAD_PREFIX[] = "\r\n\r\n";
char Message::READ_ID[] = "Read Condition";
char Message::REPLY_ID[] = "Reply Condition";
char Message::SEPARATOR[] = "\r\n";
char Message::TRAILER[] = "\r\nEND";

unsigned int Message::COLON_SPACE_LENGTH = strlen(COLON_SPACE);
unsigned int Message::SEPARATOR_LENGTH = strlen(SEPARATOR);
unsigned int Message::PADDING = strlen(COLON_SPACE) + SEPARATOR_LENGTH;
unsigned int Message::PAYLOAD_PREFIX_LENGTH = strlen(PAYLOAD_PREFIX);
unsigned int Message::TRAILER_LENGTH = strlen(TRAILER);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
//static int message_count = 0;

Message::Message(Frame *f, Channel *ch, long msgno)
  :currentFrame(NULL), channel(ch), Data(INBOUND),
  size(0), complete(false), condition(READ_ID), frames(), 
  replyCondition(REPLY_ID), headersComplete(false), numHeaders(0),
  currentHeaderData(NULL), currentHeaderDataSize(0)
{
  messageNumber = f->getMessageNumber();
  this->addFrame(f);
  dprintf("MSG CONST (inbound) CH %d ", f->getChannelNumber());
  dprintf("MSGNO %d\n", f->getMessageNumber());
  //message_count++;
  //dprintf("\nMSG *** +COUNT => %d\n", message_count);
}

Message::Message(char *data, unsigned int length, Channel *ch, 
		 char msgType, long msgno)
  :currentFrame(NULL), channel(ch), Data(OUTBOUND),
  size(0), complete(false), condition(READ_ID), frames(), 
  replyCondition(REPLY_ID), headersComplete(false), numHeaders(0),
  currentHeaderData(NULL), currentHeaderDataSize(0), type(msgType)
{
  CHANNEL_INSTANCE *channel = ch->getChannelStruct();

  if(msgno == -1)
    messageNumber = ch->getNextMessageNumber();
  else
    messageNumber = msgno;
  Frame *f = new Frame(data, length, ch, messageNumber, type, true);
  addFrame(f);  
  dprintf("MSG CONST (outbound) CH %d ", ch->getChannelNumber());
  dprintf("MSGNO %d\n", msgno);
  //message_count++;
  //dprintf("\nMSG +COUNT => %d\n", message_count);
}

Message::~Message()
{
  // Block if frames haven't been read ?? TODO Think about this
  // Iterate across our frames and free them??? I guess that's all we can do.
  Frame *temp;
  lock.lock();
  dprintf("MSG::~MSG %d\n", messageNumber);
  while(!frames.empty())
    {
      temp = frames.front();
      frames.pop_front();
      currentFrame = temp;
      delete(currentFrame);
    }
  //message_count--;
  //dprintf("\nMSG -COUNT => %d\n", message_count);

  // clean up headers
  char *c = NULL;
  for(MessageHeaderMap::iterator it = headers.begin();
      it != headers.end(); it++)
    {
      MessageHeaderMap::value_type p = *it;
      c = (char*) p.second;
      dprintf("Deleting header name=>%s", p.first.c_str());
      dprintf(" value=>%s\n", p.second);
      if(c != NULL)
	delete(c);
      string s = p.first;
      headers.erase(s);
    }
  lock.unlock();
}

//////////////////////////////////////////////////////////////////////
// Methods
//////////////////////////////////////////////////////////////////////
// Nice part, this is where the lower level read routines really
// start to pay off.  We don't do ANY offset tracking here, it's nice.

// TODO look at how fancier (well-thought-out) API's do this when 
// providing read operations on top of multiple buffers.  Ok for now.

// Also consider how Message data members could obviate some of the
// logic here - such as a 'finished' flag or something.

// Also, I think lastFrame can go away, make sure and validate
// this idea and then do/don't remove it.
int Message::read(char *buffer, unsigned int start,
		  unsigned int length)
{
  assert(dataMode == INBOUND);
  unsigned int bytesRead = 0;
  lock.lock();
  
  // Reset 'i' each time we change current frame
  unsigned int i = 0;
  Frame *lastFrame = NULL;
  while(bytesRead < length)
    {
      // Make sure we have a frame to read from
      while(currentFrame == NULL)
	{
	  if(frames.size() != 0)
	    {
	      currentFrame = frames.front();
	      frames.pop_front();
	      //dprintf("MSG Frames Size => %d\n", frames.size());
	    }
	  else
	    {
	      condition.wait(&lock);
	    }
	}
      // Read ahead...
      i = currentFrame->read(buffer, start+i, length);      
      if(i != Data::DATA_EOF)
	{
	  bytesRead += i;
	}
      else
	{
	  // We've reached the end of what's available for that frame
	  // Get rid of the frame we no longer need.
	  
	  //dprintf("MSG::Read deleting currentFrame\n", NULL);
	  // And if it's the last one, then return, cuz we're done
	  if(currentFrame->isLast())
	    {
	      delete(currentFrame);
	      currentFrame = NULL;
	      lock.unlock();
	      return bytesRead;
	    }
	  // Keep reading if it's not the last one and we've got space above
	  else
	    {
	      delete(currentFrame);
	      currentFrame = NULL;
	      start += bytesRead;
	    }
	}
    }
  if (bytesRead == length) {
    delete(currentFrame);
    currentFrame = NULL;
  }
  //dprintf("MSG::Read complete\n", NULL);
  lock.unlock();
  return bytesRead;
}

// Called from Channel, Channel has to delete the frame
// when it's done sending
Frame *Message::getNextOutboundFrame()
{
  Frame *result = NULL;
  assert(dataMode == OUTBOUND);
  lock.lock();
  if(frames.size() != 0)
    {
      result = frames.front();
      frames.pop_front();
      //dprintf("MSG Frames Size =>%d\n",frames.size());
    }
  lock.unlock();
  return result;
}

unsigned int Message::getFrameCount()
{
	return frames.size();
}

void Message::addFrame(Frame *f)
{
  //dprintf("AF1 Frames Size =>%d\n",frames.size());
  assert(complete == false);
  lock.lock();
  
  // Kinda tricky, basically bring the CRLF back for processing, but ignore
  // the CRLFEND trailer...make sense?  The CRLF preceeding header fields is
  // necessary to find them, and not including the trailer is necessary in not
  // prematurely terminating them (in case the lib splits the header across
  // multiple frames for instance) (TRAILER LENGTH ALREADY CONSIDERED in Frame::Frame)
  dprintf("Assigning Frame %x ", f);
  dprintf("msgno of %d\n", messageNumber);
  f->setMessageNumber(messageNumber);
  if(!headersComplete)
    scanForHeaders((char*)f->getData(), f->getSize(), true);
  //		scanForHeaders((char*)f->getData() - Message::SEPARATOR_LENGTH, 
  //		       f->getDataSize() + Message::SEPARATOR_LENGTH, true);
  
  frames.push_back(f);
  size += f->getSize();
  complete = f->isLast();
  lock.unlock();
  condition.notify();
  //dprintf("Leaving AddFrame\n", NULL);
}

Message &Message::operator<<(Frame *f)
{
  this->addFrame(f);
  return *this;
}

Message &Message::operator>>(Frame *&f)
{
  f = this->getNextOutboundFrame();
  return *this;
}

char *Message::getValue(char *key)
{	
  MessageHeaderMap::iterator it = headers.find((string)key);
  if(it != headers.end())
    {
      return (char*)(*it).second;	
    }
  return NULL;
}

// Length should already have discounted trailer influence
// if it was there, so we don't have to worry about it
void Message::scanForHeaders(char *data, unsigned int length, bool copy)
{
	char *temp = NULL;
	unsigned int separatorLocation, limit;
	bool append = false;

	if(length == 0)
		return;

	assert(length > 0);
	
	// If the next bit is CRLF then we're done.
	if( data[0] == Message::LINEFEED && 
		data[1] == Message::NEWLINE)
	{
		headersComplete = true;
		return;
	}

	// Either copy this frames data (simplifies the code later, even if it's
	// less performant - I have an older version that does it but it's hairy)
	if(currentHeaderData == NULL)
	{
		currentHeaderData = extractString(data, length);
		currentHeaderDataSize = length;
	}
	// Or, append this frame's data to previous data, if there is any
	// (unless it's already been done and copy is false).
	else
	{
		append = true;
		if(copy)
		{
			limit = currentHeaderDataSize + length;
			temp = new char[limit + 1];
			strncpy(temp, currentHeaderData, currentHeaderDataSize);
			strncpy(&temp[currentHeaderDataSize], data, length);
			temp[limit] = 0;

			// Prevent a leak but avoid deleting raw data from the
			// first frame (points to original stuff)
			delete[](currentHeaderData);

			// Reassign for later as a data member
			currentHeaderData = temp;
			currentHeaderDataSize = limit;
			temp = NULL;
		}
		else
		{
			limit = currentHeaderDataSize - Message::SEPARATOR_LENGTH;
			temp = new char[limit + 1];
			strncpy(temp, &currentHeaderData[Message::SEPARATOR_LENGTH], limit);
			temp[limit] = 0;

			// Prevent a leak but avoid deleting raw data from the
			// first frame (points to original stuff)
			delete[](currentHeaderData);

			// Reassign for later as a data member
			currentHeaderData = temp;
			currentHeaderDataSize = limit;
			temp = NULL;
		}
	}

	// Now we have a blob of data to scan.
	// Find the next CRLF pair
	separatorLocation = locateSeparator(currentHeaderData, currentHeaderDataSize);

	// If we're at the end...
	if(separatorLocation == 0)
	{
		delete[](currentHeaderData);
		currentHeaderData = NULL;
		currentHeaderDataSize = 0;
		return;
	}

	// Our data members hold the information, so this will stick around intra-frame
	if(separatorLocation == (unsigned int)NOT_FOUND_ERROR)
	{
	  //dprintf("Encountered EndOfFrame");
		// currentHeaderData already contains everything we'll need later, and
		// this will be called from addFrame with copy == true, so the rest of
		// the data will be appended.
		return;
	}
	assert(extractNextHeader(currentHeaderData, 0, separatorLocation) != -1);

	// Reset our tracking vars so the next header can get detected

	// If we've used all available data (the usual case for simple frames)
	// Then just reset it all.
	if(currentHeaderDataSize == separatorLocation)
	{
		delete[](currentHeaderData);
		currentHeaderData = NULL;
		limit = currentHeaderDataSize;
		currentHeaderDataSize = 0;
	}
	// If there's leftover data, then copy it to a new buffer and 
	// smoke the old one.  TODO this copying is brutal, I'm sure
	// there are better ways (MIME header Parser plugin probably) to deal
	// with this.  A big frame with lots of little headers will make
	// this look bad, but I HAVE to copy because of the constraints of
	// the underlying frame allocation scheme in C beepcore
	else
	{
		limit = currentHeaderDataSize - separatorLocation;
		temp = extractString(&currentHeaderData[separatorLocation], limit);

		// Delete the previous currentHeaderData
		delete[](currentHeaderData);

		// Reset the variables for calling later
		currentHeaderData = temp;
		currentHeaderDataSize = limit;
	}

	// Recurse
	// next starting position is currentHeaderDataSize - separatorLocation
	// Limit is the old value for currentHeaderDataSize
	if(!append)
	{
		// If we're still processing the data we received then...
		limit = length - separatorLocation;
	}
	else
	{
		// If we're operating based on stuff we had appended to, then calculate
		// Bytes left over in the buffer which HAVE to be from this frame
		// or it wouldn't have triggered it.
		limit = currentHeaderDataSize;
		separatorLocation = length - limit;  // Pos in data where these start
		assert(limit < (unsigned int)-1);
	}
	//dprintf("Recalling with %d >>%s<<\n", limit, &data[separatorLocation]);
	scanForHeaders(&data[separatorLocation], limit, false);
}

unsigned int Message::locateSeparator(char *data, unsigned int length)
{
	for(unsigned int i=0; i<length-1; i++)
	{
		if(data[i] == Message::LINEFEED &&
			data[i+1] == Message::NEWLINE)
			return i;
	}
	return (unsigned int)NOT_FOUND_ERROR;
}

unsigned int Message::extractNextHeader(char *data, unsigned int offset, 
					unsigned int maxLen)
{
	unsigned int result = (unsigned int)-1, pos = offset, original = offset;
	unsigned int separator = 0;

	// pos should now point to the start of the next entity header (if any)
	original = pos;
	while(pos < maxLen && separator == 0)
	{
		if(data[pos] == Message::COLON_SPACE[0])
			separator = pos + Message::COLON_SPACE_LENGTH;
		pos++;
	}
	if(separator == 0)
		return result;

	// Extract the name and value without munging the original string
	unsigned int length = separator - original - Message::COLON_SPACE_LENGTH;
	char *headerName = extractString(&data[original], length);

	// Pos is already at the end...
	length = maxLen - separator;
	char *headerValue = extractString(&data[separator], length);
	
	// We've got it all, print 'em out for fun and set the position
	//dprintf("Extracted=>{%s,%s}\n", headerName, headerValue);
	// Add the values to the map
	headers.insert(MessageHeaderMap::value_type((string)headerName, headerValue));
	numHeaders++;
	delete(headerName);
	//MessageHeaderMap::iterator it = headers.find(*(new string(headerName)));
	//assert(it != headers.end());
	//dprintf("Stored entity header>>%s<< with value >>%s<<\n", 
	//(char*)(*it).first.c_str(), (char*)(*it).second);
	result = length;
	return result;
}

// THESE ARE STATIC METHODS, PARSING ROUTINE, BE WARNED
char *Message::extractString(char *original, unsigned int length)
{
	char *result = new char[length+1];
	strncpy(result, original, length);
	result[length] = 0;
	return result;
}

// THESE ARE STATIC METHODS, PARSING ROUTINE, BE WARNED
void Message::trimTrailer(char *data, unsigned int length)
{
	int i=length;
	while(data[i] != Message::LINEFEED)
	{
		data[i--]=0;
	}
	data[i]=0;
}

unsigned int Message::getDataOffset(char *data, unsigned int length)
{
	for(int i=3; i < length; i++)
	{
		if( data[i] == Message::NEWLINE && 
			data[i-2] == Message::NEWLINE &&
			data[i-1 ] == Message::LINEFEED && 
			data[i-3] == Message::LINEFEED)
			return i+1;
	}
	return (unsigned int)-1;
}
