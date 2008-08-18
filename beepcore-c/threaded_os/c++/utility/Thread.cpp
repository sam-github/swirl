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
// Thread.cpp: implementation of the Thread class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(WIN32)
#include <pthread.h>
#endif
#include "Thread.h"
#include "Common.h"
extern "C"
{
//  void sched_yield();
  void pthread_kill_other_threads_np();
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
ThreadList allThreads = ThreadList();

#if defined(WIN32)
Thread::Thread(DWORD handle)
#else
Thread::Thread(pthread_t handle)
#endif
  :id(handle)
{
  // Set ID (done above)
  dprintf("New Thread with id of %d\n", id);
  // Add self to list of all threads
}

Thread::~Thread()
{
  // Remove self from list of all threads
}

void Thread::sleep(unsigned int milliseconds)
{
#if defined(WIN32)
  Sleep(milliseconds);
#else
  struct timespec st, st2;
  int seconds = milliseconds/1000;
  milliseconds -= seconds * 1000;
  st.tv_sec = seconds;
  st.tv_nsec = milliseconds * 1000;
  // I guess it returns and we should swap but who cares, find something else TODO
  nanosleep(&st, &st2);
#endif
}

void Thread::stop(Thread *t)
{
  // Stop this thread
#ifdef WIN32
#else
  pthread_cancel(t->id);
#endif
}

void Thread::yield()
{
  Thread::sleep(1);
#if defined(WIN32)
#else
  sched_yield();
#endif
}

void *Thread::join(Thread *t)
{
  void *result;
#if defined(WIN32)
  // TODO Flesh out
	result = NULL;	// gets rid of a warning
#else

  pthread_join(t->getId(), &result);
#endif
  return result;
}

void Thread::killAllThreads()
{
  // Iterate over list and call exit on each
#ifdef WIN32
  // TODO Flesh out
#else
//  pthread_kill_other_threads_np();
#endif
}

#if defined(WIN32)
Thread *Thread::startThread(void *(WINAPI *func)(void* arg), 
			    void *data)
#else
Thread *Thread::startThread(void *(*func)(void *d), 
			    void *data)
#endif
{
#if defined(WIN32)
  DWORD thread_id;
  CreateThread(NULL, NULL, (DWORD (WINAPI *)(void*)) func, data, 0, &thread_id);
#else
  pthread_t thread_id;
  pthread_create(&thread_id, NULL, func, data);
#endif
  Thread *t = new Thread(thread_id);
  return t;
}

// TODO steal registration from C libraries, store it, invoke it,
// then do our work and then close
void sigAbortHandler()
{
}

// TODO see previous routine
void sigTermHandler()
{
}
