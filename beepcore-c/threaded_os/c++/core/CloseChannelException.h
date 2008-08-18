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
// CloseChannelException.h: interface for the CloseChannelException class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CLOSECHANNELEXCEPTION_H__76AE37BF_31EB_4425_BD29_E4BE46905C16__INCLUDED_)
#define AFX_CLOSECHANNELEXCEPTION_H__76AE37BF_31EB_4425_BD29_E4BE46905C16__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BEEPException.h"

class CloseChannelException : public virtual BEEPException  
{
public:
	CloseChannelException();
	virtual ~CloseChannelException();
};

#endif // !defined(AFX_CLOSECHANNELEXCEPTION_H__76AE37BF_31EB_4425_BD29_E4BE46905C16__INCLUDED_)
