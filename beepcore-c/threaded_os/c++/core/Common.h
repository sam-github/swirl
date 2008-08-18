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
// Common.h: interface for the Common class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(CB_COMMON)
#define CB_COMMON

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../utility/semutex.h"

//#include <iostream.h>
class Frame;


//////////////////////////////////////////////////////////////////////
// Static Constants
//////////////////////////////////////////////////////////////////////
#if !defined(ALREADY_EXISTS_ERROR)
#define ALREADY_EXISTS_ERROR -1
#define NOT_FOUND_ERROR -2
#endif

#if !defined(DEBUG)
//#if defined(DEBUG)
#define dprintf(s,ar)
#else
#define dprintf(s,ar) printf(s,ar)
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Data Members
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Static Data Members
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Static Methods
//////////////////////////////////////////////////////////////////////
extern void tool();

#if defined(WIN32)

#define bcopy( x, y, z )	memcpy( y, x, z )

#else
extern "C"
{
	void bcopy(const void *src, void *dst, size_t n);
}
#endif

#endif

