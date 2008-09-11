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
 * $Id: base64.h,v 1.2 2001/10/30 06:00:37 brucem Exp $
 *
 * base64.h
 *
 * Base64 encoding and decoding.  Uses memory allocation 
 * consistent with our usage.
 *
 */
#ifndef __BASE64_H__
#define __BASE64_H__

/*  char * __base64_h_ver__ = "$Id: base64.h,v 1.2 2001/10/30 06:00:37 brucem Exp $"; */

#include "CBEEPint.h"

#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif

/*
 * base64_esize, base64_dsize
 *
 * Utilities to compute the size of the encoded base64 string from 
 * the size of the input and the size of the payload from the 
 * encoded string.
 */
extern int base64_dsize(char * encoded) ;
extern int base64_esize(int size);

/*
 * base64_encode, base64_decode, base64_decode_into.
 *
 * Perform base64 manipulation on the input char* and allocate 
 * memory for the result, which is returned.  The only variance 
 * from any common library for this is that we wrap the memory 
 * managment.
 *
 * base64_decode_into
 *    Assumes that you have allocates a buffer as a destination of the
 *    correct size.  If this is not the case you will probably SEGV.
 */
extern char * base64_encode(struct session * sesion, char * raw, int size);
extern char * base64_encode_into(char * raw, int size, unsigned char * output);
extern int base64_decode(struct session * sesion, char * encoded, char ** raw);
extern int base64_decode_into(struct session * session, char * encoded, char * raw);

#define BASE64_LINELEN   76

/* #endif __BASE64__ */
#endif
