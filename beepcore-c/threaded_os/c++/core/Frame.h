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
// Frame.h: interface for the Frame class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_FRAME_H__90EAF252_80F6_4781_942E_94E133065F81__INCLUDED_)
#define AFX_FRAME_H__90EAF252_80F6_4781_942E_94E133065F81__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Data.h"
class Channel;
class Session;

#ifdef WIN32
#pragma warning(disable:4786)
#include <windows.h>
#include "../../utility/semutex.h"
#endif

extern "C"
{
#include "../../core/generic/CBEEP.h"
#include "../wrapper/bp_wrapper.h"
}

// If you want to modify the functionality, modify these defines
#define EXPECT_BEEP_HEADER
#define PRUNE_TRAILER

class Frame : public virtual Data  
{
  friend class Channel;
  friend class Message;
  friend class Session;
 public:
  Frame(struct frame *f, CHANNEL_INSTANCE *channel);
  Frame(const char *data, unsigned int length, Channel *ch,
	long messageNumber, char type, bool last);
  virtual ~Frame();
  bool isLast();
  
  virtual int read(char *buffer, unsigned int start,
		   unsigned int length);
  
  virtual long getAnswerNumber(){return frameStruct->answer_number;}
  virtual long getChannelNumber(){return frameStruct->channel_number;}
  virtual long getMessageNumber(){return frameStruct->message_number;}
  virtual char getMessageType(){return frameStruct->msg_type;}
  virtual char *getData(){return frameStruct->payload;}
  virtual long getSize(){return frameStruct->size;}
  // Constants
  static char LAST, MORE, ANSWER_TYPE, ERROR_TYPE, NULL_TYPE, 
    MESSAGE_TYPE, REPLY_TYPE;
  
 protected:
  struct frame *getFrameStruct(){return frameStruct;}
  void setMessageNumber(long num)
    {frameStruct->message_number = num;}
 private:
  struct frame *frameStruct;
  CHANNEL_INSTANCE *channel;
  static unsigned int getDataOffset(char *data, unsigned int length);
};

#endif // !defined(AFX_FRAME_H__90EAF252_80F6_4781_942E_94E133065F81__INCLUDED_)
