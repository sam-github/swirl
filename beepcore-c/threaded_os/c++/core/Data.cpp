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
// Data.cpp: implementation of the Data class.
//
//////////////////////////////////////////////////////////////////////

#include "Common.h"
#include "Data.h"
#include "bp_config.h"

//////////////////////////////////////////////////////////////////////
// Static Constants
//////////////////////////////////////////////////////////////////////
int Data::DATA_EOF = -1;
char Data::INBOUND = 'i';
char Data::OUTBOUND = 'o';

//////////////////////////////////////////////////////////////////////
// Data Members
//////////////////////////////////////////////////////////////////////
/**
 * This is provided here to make it happen once...Superclasses that 
 * override this should...override it.
 */
void Data::reset()
{
  offset = 0;
}

//////////////////////////////////////////////////////////////////////
// Static Methods
//////////////////////////////////////////////////////////////////////
/**
 * This is the meta-read function, for simplicity, but not necessarily
 * efficiency, I use this basically everywhere, frames, messages, etc.
 */ 
int Data::read(char *writeBuff, unsigned int writePos,
	       unsigned int writeLength, char *readBuff,		
	       unsigned int &startReadPos, unsigned int readLimitPos)
{
  // If all bytes have been read (meaning that the current offset is at 
  // the limit position), then do nothing and return.
  if(startReadPos == readLimitPos)
    return Data::DATA_EOF;
  
  // Otherwise, there is data to be read
  // Lock to prevent concurrent reads and associated complications
  lock();

  // Reading data - now we have to figure out how much, by first
  // computing what's available and comparing that to how much
  // buffer space the reader has provided.  We'll either read all
  // of what's left to read, or as much as the reader can handle.
  unsigned int bytesAvailable = readLimitPos - startReadPos;
  if(writeLength > bytesAvailable)
    writeLength = bytesAvailable;

  // Copy 'writeLength' bytes into 'writeBuff' from 'readBuff' using
  // the position parameters to generate pointer offsets.
  bcopy((const void*)&readBuff[startReadPos], (void*)&writeBuff[writePos], (size_t)writeLength);
	
  // Increment offset for our subclasses (this will probably just be
  // &offset in reality ;) so I might even be able to eliminate that parm)
  // and just use offset here.
  startReadPos += writeLength;

  // Unlock
  unlock();

  // Return the amount of data written
  return writeLength;
}
