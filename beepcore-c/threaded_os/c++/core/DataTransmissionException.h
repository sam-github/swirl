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
// DataTransmissionException.h: interface for the DataTransmissionException class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DATATRANSMISSIONEXCEPTION_H__62D1EF62_9BED_40FD_8AE7_0B84A6F8668E__INCLUDED_)
#define AFX_DATATRANSMISSIONEXCEPTION_H__62D1EF62_9BED_40FD_8AE7_0B84A6F8668E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BEEPException.h"

class DataTransmissionException : public BEEPException  
{
public:
	DataTransmissionException();
	virtual ~DataTransmissionException();

};

#endif // !defined(AFX_DATATRANSMISSIONEXCEPTION_H__62D1EF62_9BED_40FD_8AE7_0B84A6F8668E__INCLUDED_)
