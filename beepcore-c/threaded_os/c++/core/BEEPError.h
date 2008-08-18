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
// BEEPError.h: interface for the BEEPError class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BEEPERROR_H__CD58C164_65C7_498F_8548_9692ED04A520__INCLUDED_)
#define AFX_BEEPERROR_H__CD58C164_65C7_498F_8548_9692ED04A520__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BEEPException.h"

class BEEPError : public BEEPException  
{
public:
	BEEPError();
	virtual ~BEEPError();

};

#endif // !defined(AFX_BEEPERROR_H__CD58C164_65C7_498F_8548_9692ED04A520__INCLUDED_)
