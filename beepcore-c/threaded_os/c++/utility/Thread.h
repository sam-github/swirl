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
// Thread.h: interface for the Thread class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_THREAD_H__9D6BEFE5_28D2_437C_9F47_33F253E15527__INCLUDED_)
#define AFX_THREAD_H__9D6BEFE5_28D2_437C_9F47_33F253E15527__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class Thread;

#if defined(PTHREAD)
#include <pthread.h>
#elif defined(WIN32)
#pragma warning(disable:4786)
#include <windows.h>
#endif

#include <list>
using namespace std;
typedef list<Thread*> ThreadList;

#include "Mutex.h"

class Thread
{
public:
	virtual ~Thread();

	static void exit(int code);
	static void killAllThreads();
	static void sleep(unsigned int milliseconds);
	static void yield();
	static void *join(Thread *t);

#if defined(WIN32)
	static Thread *startThread( void* (WINAPI *f)(void *arg), void *data);
#else
	static Thread *startThread(void *(*f)(void *arg), void *data);
#endif

	static void stop(Thread *t);

#if defined(WIN32)
	DWORD getId() { return id; }
#else
	pthread_t getId() { return id; }
#endif

protected:

#if defined(WIN32)
	Thread::Thread(DWORD handle);
	DWORD id;
#else
	Thread::Thread(pthread_t handle);
	pthread_t id;
#endif

private:
	Mutex mutex;
	unsigned long stackSize;

	static ThreadList allThreads;
	static void signalAbortHandler();
	static void signalTermHandler();
};

#endif // !defined(AFX_THREAD_H__9D6BEFE5_28D2_437C_9F47_33F253E15527__INCLUDED_)
