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
 * IW_malloc.c
 *
 * Implements the global definitions for the memory management
 * abstraction.  
 */

#include "bp_malloc.h"

static const char * _iw_malloc_c_ver = "$Id: bp_malloc.c,v 1.2 2001/10/30 06:00:40 brucem Exp $";

static blw_fp_malloc lib_malloc_fp = NULL;
static blw_fp_free lib_free_fp = NULL;

int lib_malloc_init(blw_fp_malloc newmalloc, blw_fp_free newfree)
{
    if (NULL == _iw_malloc_c_ver) {};

    if ((NULL == lib_malloc) || (NULL == lib_free))
	return(LIB_MALLOC_ERROR);
    
    lib_malloc_fp = newmalloc;
    lib_free_fp = newfree;

    if ((NULL == lib_malloc) || (NULL == lib_free))
	return(LIB_MALLOC_ERROR);

    return(LIB_MALLOC_OK);
}

void * lib_malloc(size_t size) 
{
    if (NULL != lib_malloc_fp) {
	return((*lib_malloc_fp)(size));
    } else {
	return(NULL);
    }
}

void lib_free(void *ptr)
{
    if (NULL != lib_free_fp) {
	(*lib_free_fp)(ptr);
    }
    return;
}

