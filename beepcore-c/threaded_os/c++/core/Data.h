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
// Data.h: interface for the Data class.
//
// This class is merely used to associate the general data type
// extended by Frame and Message
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DATA_H__AEAE54BE_4E34_4F88_8C0D_40C087649558__INCLUDED_)
#define AFX_DATA_H__AEAE54BE_4E34_4F88_8C0D_40C087649558__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Mutex.h"

class Data
{
public:
	Data(char mode){dataMode = mode; offset=0;};
	virtual ~Data(){}
	virtual void lock() {}
	virtual void unlock() {}
	virtual int read(char *buffer, unsigned int start,
			 unsigned int length) = 0;
	virtual long getCurrentOffset(){return offset;}
	virtual long getSize() = 0;
	virtual void reset();

	static int DATA_EOF;
	static char INBOUND, OUTBOUND;
protected:
	virtual int read(char *writeBuff, unsigned int writePos,
					 unsigned int writeLength, char *readBuff,
					 unsigned int &startReadPos, 
					 unsigned int readLimitPos);
	Mutex lockData;
	char dataMode;
	unsigned int offset;
};

#endif // !defined(AFX_DATA_H__AEAE54BE_4E34_4F88_8C0D_40C087649558__INCLUDED_)
