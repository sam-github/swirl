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
 * $Id: cbxml_entities.h,v 1.3 2001/10/30 06:00:37 brucem Exp $
 *
 * xml_entities.h
 *
 * Routines for handling XML entities, knowing that we have 
 * malloc restrictions. 
 *
 */

#ifndef __CBXML_ENTITIES_H__
#define __CBXML_ENTITIES_H__

#include "CBEEPint.h"

/*
 * Inclusion is the substitution of XML entites with characters,
 * and these routiens handle the rquired 5 needed by the XML spec, which
 * are <>'"&.
 *
 * include:
 *    Given a session struct to determine the malloc call and the input string, 
 *    returns a new string with the included input string.  
 *    Returns NULL if memory allocation fails.
 *
 * include_length:
 *    Given the input string, returns the length of that string when included.
 *
 * include_isneeded:
 *    Given the input string, returns 1 if characters requiring inclusion are 
 *    found.
 *
 * include_inplace:
 *    Given the input string, this routine performs the inclusion in place.
 */
extern char * cbxml_include(struct session * session, char * in);
extern int cbxml_include_length(char * in);
extern int cbxml_include_isneeded(char * in);
extern int cbxml_include_inplace(char * in);
extern int cbxml_include_into(char * in, char * out);
/*
 * Normalization is the substitution of characters with XML entites,
 * and these routiens handle the rquired 5 needed by the XML spec, which
 * are <>'"&.
 *
 * normalize:
 *    Given a session struct to determine the malloc call and the input string, 
 *    returns a new string with the normalized input string.  
 *    Returns NULL if memory allocation fails.
 *
 * normalize_length:
 *    Given the input string, returns the length of that string when normalized.
 *
 * normalize_isneeded:
 *    Given the input string, returns 1 if entities requiring normalization are 
 *    found.
 *
 * normalize_inplace:
 *    Given the input string, this routine assumes that sufficient string buffer
 *    spaces has already been allocated to the input string and performs the 
 *    normalization in place.
 */
extern char * cbxml_normalize(struct session * session, char * in);
extern int cbxml_normalize_length(char * in);
extern int cbxml_normalize_isneeded(char * in);
extern int cbxml_normalize_inplace(char * in);
extern int cbxml_normalize_into(char * in, char * out);


#endif
