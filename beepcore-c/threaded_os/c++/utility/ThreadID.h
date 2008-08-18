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
// ThreadID.h: interface for the ThreadID class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_THREADID_H__A25AD14D_43DE_40BA_ADC9_55D886B38407__INCLUDED_)
#define AFX_THREADID_H__A25AD14D_43DE_40BA_ADC9_55D886B38407__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Common.h"

#if defined(WIN32)
#include <windows.h>
#elif defined(PTHREAD)
#include <pthread.h>
#endif

class ThreadID  
{
 public:
  ThreadID();
  virtual ~ThreadID(){}
  void reinitialize();

 protected:
#if defined(WIN32)
  DWORD	tid;
  DWORD getTid(){return tid;}
  //void setTid(DWORD t){tid = t;}
#elif defined(PTHREAD)
  int tid;   //	thread_t tid;
  int getTid(){return tid;}
  //void setTid(int t){tid = t;}
#endif 

  // const ThreadID& operator=(ThreadID& ti)
  ThreadID& operator=(ThreadID& ti)
  {
    tid = ti.getTid();
    return *this;
  }

  //  friend int operator==(const ThreadID& ti, const ThreadID& tj)
  friend int operator==(ThreadID& ti, ThreadID& tj)
  {
    return(ti.getTid() == tj.getTid());
  }
};

#endif // !defined(AFX_THREADID_H__A25AD14D_43DE_40BA_ADC9_55D886B38407__INCLUDED_)
