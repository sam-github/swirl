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
// Mutex.cpp: implementation of the Mutex class.
//
//////////////////////////////////////////////////////////////////////

#include "Mutex.h"
#include "Common.h"

////////////////q//////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Mutex::Mutex()
{
#ifdef WIN32
	InitializeCriticalSection(&critSect);
#else
	pthread_mutex_init(&mutexHandle, NULL);
#endif
}

Mutex::~Mutex()
{
#ifdef WIN32
	DeleteCriticalSection(&critSect);
#else
	pthread_mutex_destroy(&mutexHandle);
#endif
}

void Mutex::lock()
{
#ifdef WIN32
	EnterCriticalSection(&critSect);
#else
	//dprintf("Mutex Lock %x\n", this);
	pthread_mutex_lock(&mutexHandle);
#endif
}

void Mutex::unlock()
{
#ifdef WIN32
	LeaveCriticalSection(&critSect);
#else
	//dprintf("Mutex unlock %x\n", this);
	pthread_mutex_unlock(&mutexHandle);
#endif
}
