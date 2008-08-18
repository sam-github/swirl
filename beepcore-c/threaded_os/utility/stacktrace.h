/*
 * Copyright (c) 2001 Bruce Mitchener, Jr.  All rights reserved.
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
/*
 * $Id: stacktrace.h,v 1.1 2002/04/04 22:12:34 sweetums Exp $
 *
 */
/* This file defines the interface between the wrapper and the 
   logging facility. */
#ifndef _STACKTRACE_H
#define _STACKTRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * This function will print the current stack trace.
 *
 * 
 * @param prefix      Prefix each line of the stack trace with the given string.
 *
 **/
extern void bp_print_stack(char* prefix);

#ifdef __cplusplus
}
#endif

/* endif multiple-include protection */
#endif
