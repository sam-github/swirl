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
// SessionContext.h: interface for the SessionContext class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SESSIONCONTEXT_H__16302612_5F6F_4834_AE2E_C4907AEC5FA7__INCLUDED_)
#define AFX_SESSIONCONTEXT_H__16302612_5F6F_4834_AE2E_C4907AEC5FA7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "bp_wrapper.h"
#include "ProfileRegistry.h"

// A session is associated with ONE SessionContext, but SessionContexts can
// be associated with multiple sessions, and peers can be associated with
// multiples of both...
class SessionContext  
{
 public:
  SessionContext(WRAPPER *w);
  virtual ~SessionContext();
  
  WRAPPER *getWrapperStruct(){return wrapper;}
  void addProfile(ProfileRegistration *pr);
  void removeProfile(ProfileRegistration *pr);

 private:
  WRAPPER *wrapper;
  ProfileRegistry *registry;
};

#endif // !defined(AFX_SESSIONCONTEXT_H__16302612_5F6F_4834_AE2E_C4907AEC5FA7__INCLUDED_)
