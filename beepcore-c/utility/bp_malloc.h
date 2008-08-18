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
/*
 * $Id: bp_malloc.h,v 1.2 2001/10/30 06:00:40 brucem Exp $
 *
 * IW_malloc.h
 *
 * Provides the global definitions for the memory management
 * abstraction.  Primarily this is the function pointers which
 * will be used in place of malloc and free.  
 */
#ifndef BP_MALLOC_H
#define BP_MALLOC_H 1

#include <stdlib.h>

typedef void * (*blw_fp_malloc)(size_t size);
typedef void (*blw_fp_free)(void *ptr);

/*
 * lib_malloc_init
 *
 * Sets the malloc and free functions to be used.
 * Returns LIB_MALLOC_ERROR if passed a null pointer for either function.
 */
extern int lib_malloc_init(blw_fp_malloc, blw_fp_free);
/*
 * Functional replacements for the malloc and free we all love.
 */
extern void * lib_malloc(size_t size);
extern void lib_free(void *ptr);

#define LIB_MALLOC_OK             0
#define LIB_MALLOC_ERROR         -1

#endif



