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
// TuningResetException.h: interface for the TuningResetException class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TUNINGRESETEXCEPTION_H__ECAAC0E7_3517_4C4B_8515_34513EA8CBD0__INCLUDED_)
#define AFX_TUNINGRESETEXCEPTION_H__ECAAC0E7_3517_4C4B_8515_34513EA8CBD0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BEEPException.h"

class TuningResetException : public BEEPException  
{
public:
	TuningResetException();
	virtual ~TuningResetException();

};

#endif // !defined(AFX_TUNINGRESETEXCEPTION_H__ECAAC0E7_3517_4C4B_8515_34513EA8CBD0__INCLUDED_)
