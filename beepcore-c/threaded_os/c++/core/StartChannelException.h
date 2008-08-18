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
// StartChannelException.h: interface for the StartChannelException class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_STARTCHANNELEXCEPTION_H__D11DB2DB_19A0_40FD_8A0B_93AF5899A90C__INCLUDED_)
#define AFX_STARTCHANNELEXCEPTION_H__D11DB2DB_19A0_40FD_8A0B_93AF5899A90C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BEEPException.h"

class StartChannelException : public virtual BEEPException  
{
public:
	StartChannelException();
	virtual ~StartChannelException();

};

#endif // !defined(AFX_STARTCHANNELEXCEPTION_H__D11DB2DB_19A0_40FD_8A0B_93AF5899A90C__INCLUDED_)
