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
 * cbxml_entities.c
 *
 * Wrapper routines for handling XML entities, knowing that we have 
 * malloc restrictions. 
 *
 */

char * __cxml_entities_c_ver__ = "$Id: cbxml_entities.c,v 1.2 2001/10/30 06:00:37 brucem Exp $";

#include "CBEEPint.h"
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <strings.h>
#endif
#include "cbxml_entities.h"
#include "../../utility/xml_entities.h"
#include "../../utility/xml_parse_constants.h"


/*
 * inclusion routines
 */
char * cbxml_include(struct session * session, char * in) {
  char * out;
  
  if ((out = session->malloc(cbxml_include_length(in) +1))) {
    xml_include_into(in, out);
  }

  return out;
}
/*
 * include_length
 */
int cbxml_include_length(char * in) {
  return xml_include_length(in);
}

/*
 * include_isneeded:
 *    Given the input string, returns 1 if entitiess requiring inclusion are 
 *    found.
 */
int cbxml_include_isneeded(char * in) {
  return xml_include_isneeded(in);
}

/*
 * include_inplace:
 *    Given the input string, this routine performs the inclusion in place.
 */
int cbxml_include_inplace(char * in) {
  return xml_include_inplace(in);
}

int cbxml_include_into(char * in, char * out) {
  return xml_include_into(in, out);
}

/*
 * normalization routines
 *
 * normalize:
 *    Given a session struct to determine the malloc call and the input string, 
 *    returns a new string with the normalized input string.  
 *    Returns NULL if memory allocation fails.
 */
char * cbxml_normalize(struct session * session, char * in) {
  char * out;
  int x;

  x = cbxml_normalize_length(in) +1;
  
  if ((out = session->malloc(x))) {
    xml_normalize_into(in, out);
  }

  return out;
}

/*
 * normalize_length:
 *    Given the input string, returns the length of that string when normalized.
 */
int cbxml_normalize_length(char * in) {
  return xml_normalize_length(in);
}

/*
 * normalize_isneeded:
 *    Given the input string, returns 1 if entities requiring normalization are 
 *    found.
 */
int cbxml_normalize_isneeded(char * in) {  
  return xml_normalize_isneeded(in);
}
/*
 * normalize_inplace:
 *    Given the input string, this routine assumes that sufficient string buffer
 *    spaces has already been allocated to the input string and performs the 
 *    normalization in place.
 */
int cbxml_normalize_inplace(char * in) {
  return xml_normalize_inplace(in);
}

int cbxml_normalize_into(char * in, char * out) {
  return xml_normalize_into(in, out);
}


