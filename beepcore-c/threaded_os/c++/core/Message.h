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
// Message.h: interface for the Message class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MESSAGE_H__A956A88E_B7A7_4A02_8047_37B67F9C212F__INCLUDED_)
#define AFX_MESSAGE_H__A956A88E_B7A7_4A02_8047_37B67F9C212F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Common.h"
#include "Frame.h"
#include "Condition.h"
#include "Mutex.h"
class Channel;

#ifdef WIN32
#pragma warning(disable:4786)
#include <windows.h>
#endif

#include <list>
#include <map>
#include <string>
using namespace std;
typedef list<Frame*> FrameList;
typedef map<string, char*> MessageHeaderMap;

class Channel;

class Message : public virtual Data  
{
  friend class Channel;
 public:
  Message::Message(Frame *f, Channel *ch, long msgno);
  Message::Message(char *data, unsigned int length,
		   Channel *ch, char msgtype, long msgno);
  // TODO
  // Message(Channel *ch);
  // Message::Message(char *data, Channel *ch);

  virtual ~Message();
  virtual int read(char *buffer, unsigned int start,
		   unsigned int length);

  virtual unsigned int getFrameCount();
  virtual void addFrame(Frame *f);
  virtual long getMessageNumber(){return messageNumber;}
  virtual bool isComplete(){return complete;}
  virtual long getSize(){return size;}

  virtual Message& operator<<(Frame *f);
  virtual Message& operator>>(Frame *&f);
  char *getHeaderValue(char *headerKey){return getValue(headerKey);}
  virtual char *getValue(char *headerKey);
  char getMessageType(){return type;}

  // Constants and Stuff
  static char NEWLINE, LINEFEED;
  static char COLON_SPACE[], PAYLOAD_PREFIX[], SEPARATOR[], TRAILER[];
  static char CONTENT_TYPE[], CONTENT_TRANSFER_ENCODING[];
  static char READ_ID[], REPLY_ID[];
  static unsigned int PADDING, PAYLOAD_PREFIX_LENGTH, SEPARATOR_LENGTH,
    COLON_SPACE_LENGTH, TRAILER_LENGTH;
 protected:
  // Called by Channel as a friend
  virtual void waitForReply(Mutex *m){replyCondition.wait(m);}
  virtual void notifyReply(){replyCondition.notify();}

  unsigned int extractNextHeader(char *data, unsigned int offset,
				 unsigned int maxLen);
  void scanForHeaders(char *data, unsigned int length, bool copy);
  unsigned int locateSeparator(char *data, unsigned int length);
  static char *extractString(char *original, unsigned int length);
  static unsigned int getDataOffset(char *data, unsigned int length);
  static void trimTrailer(char *data, unsigned int length);
  virtual Frame* getNextOutboundFrame();

 private:
  // Header Scanning
  bool headersComplete;
  char *currentHeaderData;
  MessageHeaderMap headers;
  unsigned int currentHeaderDataSize, numHeaders;

  // Tracking our Associated Channel
  Channel *channel;
  
  // For reading
  bool complete;
  Condition condition, replyCondition;
  Frame *currentFrame;
  FrameList frames;
  Mutex lock;
  char type;
  long messageNumber, size;
};

#endif // !defined(AFX_MESSAGE_H__A956A88E_B7A7_4A02_8047_37B67F9C212F__INCLUDED_)
