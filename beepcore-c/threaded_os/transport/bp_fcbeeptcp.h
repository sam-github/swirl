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
#ifndef CBEEPTCP_H
#define CBEEPTCP_H 1
#include "../../utility/bp_slist_utility.h"
#include "bp_fiostate.h"
#include "../wrapper/bp_wrapper.h"
#include "../wrapper/bp_wrapper_private.h"

/* data definition for initiating client */

#if defined(__cplusplus)
extern "C"
{
#endif

extern void notify_lower(struct session * s, int i);

#if defined(__cplusplus)
}
#endif

#endif


