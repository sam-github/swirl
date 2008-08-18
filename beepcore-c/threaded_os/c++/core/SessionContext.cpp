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
// SessionContext.cpp: implementation of the SessionContext class.
//
//////////////////////////////////////////////////////////////////////

#include "SessionContext.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SessionContext::SessionContext(WRAPPER *w)
  :wrapper(w)
{
  dprintf("New Session Context\n", NULL);
}

SessionContext::~SessionContext()
{
  // releaese the wrapper and disable self, obviously
}
