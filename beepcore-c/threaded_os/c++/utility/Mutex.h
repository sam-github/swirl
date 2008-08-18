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
// Mutex.h: interface for the Mutex class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_MUTEX_H__38B7A810_B18F_417C_A59F_BD72919A821D__INCLUDED_)
#define AFX_MUTEX_H__38B7A810_B18F_417C_A59F_BD72919A821D__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <stdio.h>

class Mutex  
{
public:
	Mutex();
	virtual ~Mutex();
	virtual void lock();
	virtual void unlock();
#ifdef WIN32
	CRITICAL_SECTION getMutexHandle();
#else
	pthread_mutex_t *getMutexHandle(){return &mutexHandle;}
#endif

protected:
#ifdef WIN32
	CRITICAL_SECTION critSect;
#else
	pthread_mutex_t mutexHandle;
#endif
};

#endif // !defined(AFX_MUTEX_H__38B7A810_B18F_417C_A59F_BD72919A821D__INCLUDED_)
