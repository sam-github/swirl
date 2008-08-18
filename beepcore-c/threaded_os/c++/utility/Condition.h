/*
 * Copyright (c) 2001 Invisible Worlds, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the Blocks Public License (the
 * "License"); You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at http://www.beepcore.org/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 */
// Condition.h: interface for the Condition class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_CONDITION_H__840F383B_4E3A_4B22_AE11_D1EC5D906557__INCLUDED_)
#define AFX_CONDITION_H__840F383B_4E3A_4B22_AE11_D1EC5D906557__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include "Mutex.h"

class Condition  
{
public:
	Condition(void *arg = NULL);
	virtual ~Condition();

	void wait(Mutex *m);
	void wait(Mutex *m, unsigned int millis);
	void notify();
private:
#ifdef WIN32
	static char identifier[];
	HANDLE conditionHandle;
#else
	pthread_cond_t conditionHandle;
#endif
};

#endif // !defined(AFX_CONDITION_H__840F383B_4E3A_4B22_AE11_D1EC5D906557__INCLUDED_)
