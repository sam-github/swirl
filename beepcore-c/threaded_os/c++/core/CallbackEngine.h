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
// CallbackEngine.h: interface for the CallbackEngine class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CALLBACKENGINE_H__F2896FA8_1C63_479B_8E29_DE0EC1832DD0__INCLUDED_)
#define AFX_CALLBACKENGINE_H__F2896FA8_1C63_479B_8E29_DE0EC1832DD0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#include "Common.h"

#if !defined(WIN32)
#include "bp_wrapper.h"
#endif

class CallbackEngine  
{
  friend class ProfileRegistration;
 public:
	CallbackEngine();
	virtual ~CallbackEngine();
 protected:
	static void initializeCallbacks(struct _profile_registration *pr);
};

#endif // !defined(AFX_CALLBACKENGINE_H__F2896FA8_1C63_479B_8E29_DE0EC1832DD0__INCLUDED_)
