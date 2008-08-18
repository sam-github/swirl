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
// ThreadID.cpp: implementation of the ThreadID class.
//
//////////////////////////////////////////////////////////////////////

#include "ThreadID.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ThreadID::ThreadID()
{
	reinitialize();
}

void ThreadID::reinitialize()
{
	// Initialize the Thread Identity
#if defined(PTHREAD)
	tid = pthread_self();
#elif defined(WIN32)
	tid = GetCurrentThreadId();
#endif
}
