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
// Condition.cpp: implementation of the Condition class.
//
//////////////////////////////////////////////////////////////////////

#include <time.h>
#include "../core/Common.h"
#include "Condition.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
#ifdef WIN32
char Condition::identifier[] = "C++ Peer Wrapper Condition";
#endif

Condition::Condition(void *arg)
{
#ifdef WIN32
  // Using auto-resetting events that aren't initially signalled
  conditionHandle = CreateEvent(NULL, false, false, (char*)arg);
#else
  pthread_cond_init(&conditionHandle, NULL);	
#endif
#ifdef DEBUG
  //dprintf("\nNew Condition %x %s", arg, conditionHandle);
#endif
}

Condition::~Condition()
{
#ifdef WIN32
  CloseHandle(conditionHandle);
#else
  pthread_cond_destroy(&conditionHandle);
#endif
}

void Condition::wait(Mutex *m)
{
#ifdef WIN32
	wait( m, INFINITE );
//  m->unlock();
//  wait(INFINITE);
#else
  if(m != NULL)
    pthread_cond_wait(&conditionHandle, m->getMutexHandle());
  else
    //    dprintf("Condition wait %x\n", this);
    pthread_cond_wait(&conditionHandle, NULL);
  //    dprintf("Condition done with wait %x\n", this);
#endif
}

void Condition::wait(Mutex *m, unsigned int millis)
{
#ifdef WIN32
  m->unlock();
  DWORD i = WaitForSingleObject(conditionHandle, millis);
  //dprintf("Wait=>%x %x %x %x\n",i, WAIT_ABANDONED, WAIT_OBJECT_0, WAIT_TIMEOUT);
#else
/* Wait for condition variable COND to be signaled or broadcast until
   ABSTIME.  MUTEX is assumed to be locked before.  ABSTIME is an
   absolute time specification; zero is the beginning of the epoch
   (00:00:00 GMT, January 1, 1970).  */
  struct timespec ts;
  // TODO his comes in as milliseconds (maybe we change that...)
  if(millis > 1000)
    {
      // So first, figure out how many seconds it is by /1000
      // Then modify millis so it no longer contains seconds
      ts.tv_sec = millis/1000;
      millis = millis % 1000;
    }	
  // And set the nanoseconds from 10x3 to 10x9
  ts.tv_nsec = millis * 1000000;
  assert(m != NULL);
  pthread_cond_timedwait(&conditionHandle, 
			 m->getMutexHandle(), 
			 &ts);
#endif
}

void Condition::notify()
{
#ifdef WIN32
  SetEvent(conditionHandle);
#else
  //  dprintf("Condition notify %x\n", this);
  pthread_cond_signal(&conditionHandle);
  //  dprintf("Condition signalled %x\n", this);
#endif
}
