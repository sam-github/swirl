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
// DataListener.h: interface for the DataListener class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DATALISTENER_H__E7FD02E4_C4E9_45A1_A626_90A08E667561__INCLUDED_)
#define AFX_DATALISTENER_H__E7FD02E4_C4E9_45A1_A626_90A08E667561__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Channel;
class Frame;

class DataListener  
{
public:
	DataListener();
	virtual ~DataListener();
	virtual void receiveFrame(Frame *f, Channel *ch) = 0;
};

#endif // !defined(AFX_DATALISTENER_H__E7FD02E4_C4E9_45A1_A626_90A08E667561__INCLUDED_)
