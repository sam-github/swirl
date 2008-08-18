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
 * xml_entities.c
 *
 * Routines for handling XML entities, knowing that we have 
 * malloc restrictions. 
 *
 */

char * __xml_entities_c_ver__ = "$Id: xml_entities.c,v 1.2 2001/10/30 06:00:40 brucem Exp $";

#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <strings.h>
#endif
#include "bp_malloc.h"
#include "xml_entities.h"
#include "xml_parse_constants.h"



/*
 * prototypes
 */
int xml_include_worker(char * in, char * out);
int xml_normalize_worker(char * in, char * out, int outlen);

/*
 * inclusion routines
 */
char * xml_include(char * in) {
  char * out;
  
  if ((out = lib_malloc(xml_include_length(in) +1))) {
    xml_include_worker(in, out);
  }

  return(out);
}
/*
 * include_length
 */
int xml_include_length(char * in) {
  int size = 0, mode = 1; /* 1 = outside <![CDATA[, 2 = inside */

  while (*in) {
    if (mode==1) {
      if ('&' == *in) {
	switch (in[1]) {
	case 'l':
	  if (0==strncmp(in, "&lt;", 4)) {in += 3;}
	  break;
	case 'g':
	  if (0==strncmp(in, "&gt;", 4)) {in += 3;}
	  break;
	case 'q':
	  if (0==strncmp(in, "&quot;", 6)) {in += 5;}
	  break;
	case 'a':
	  if (0==strncmp(in, "&amp;", 5)) {in += 4;}
	  else if (0==strncmp(in, "&apos;", 6)) {in += 5;}
	  break;
	default:
	  break;
	}
      }
      else if (('<' == *in) && (0==strncmp(in, "<![CDATA[",9))) {
	mode = 0; 
	in += 8;
	size += 8;
      } 
    } else {
      if ((']' == *in) && (0 == strncmp(in, "]]>", 3))) {
	mode = 1; 
	in += 2;
	size += 2;
      }
    }
    in += 1;
    size++;
  }

  return(size);
}

/*
 * include_isneeded:
 *    Given the input string, returns 1 if entitiess requiring inclusion are 
 *    found.
 */
int xml_include_isneeded(char * in) {

  int mode = 1; /* 1 = outside <![CDATA[, 2 = inside */

  while (*in) {
    if (mode==1) {
      if ('&' == *in) {
	switch (in[1]) {
	case 'l':
	  if (0==strncmp(in, "&lt;", 4)) 
	    return(1);
	  break;
	case 'g':
	  if (0==strncmp(in, "&gt;", 4))
	    return(1);
	  break;
	case 'q':
	  if (0==strncmp(in, "&quot;", 6))
	    return(1);
	  break;
	case 'a':
	  if (0==strncmp(in, "&amp;", 5))
	    return(1);
	  else if (0==strncmp(in, "&apos;", 6)) 
	    return(1);
	  break;
	default:
	  break;
	}
      }
      else if (('<' == *in) && (0==strncmp(in, "<![CDATA[", 9))) {
	mode = 2; in += 8;
      } 
    } else {
      if ((']' == *in) && (0 == strncmp(in, "]]>", 3))) {
	mode = 1; in += 2;
      } 
    }
    in += 1; 
  }

  return(0);
}

/*
 * include_inplace:
 *    Given the input string, this routine performs the inclusion in place.
 */
int xml_include_inplace(char * in) {
  return(xml_include_worker(in, in));
}

int xml_include_into(char * in, char * out) {
  return(xml_include_worker(in, out));
}

/*
 * worker routine that really does the job.
 */
int xml_include_worker(char * in, char * out) {
  int size = 0, mode = 1; /* 1 = outside <![CDATA[, 2 = inside */

  while (*in) {
    if (mode==1) {
      if ('&' == *in) {
	switch (in[1]) {
	case 'l':
	  if (0==strncmp(in, "&lt;", 4)) {*out = '<'; in += 3;}
	  break;
	case 'g':
	  if (0==strncmp(in, "&gt;", 4)) {*out = '>'; in += 3;}
	  break;
	case 'q':
	  if (0==strncmp(in, "&quot;", 6)) {*out = '&'; in += 5;}
	  break;
	case 'a':
	  if (0==strncmp(in, "&amp;", 5)) {*out = '&'; in += 4;}
	  else if (0==strncmp(in, "&apos;", 6)) {
	    *out = '\''; 
	    in += 5;
	  }
	  break;
	default:
	  break;
	}
      }
      else if (('<' == *in) && (0==strncmp(in, "<![CDATA[", 9))) {
	mode = 2; in += 8;
      } else {
	*out = *in;
      }
    } else {
      if ((']' == *in) && (0 == strncmp(in, "]]>", 3))) {
	mode = 1; in += 2;
      } else {
	*out = *in;
      }
    }
    size++;
    in += 1; 
    out += 1;
  }

  *out = '\0';
  return size;
}

/*
 * normalization routines
 *
 * normalize:
 *    Returns a new string with the normalized input string.  
 *    Returns NULL if memory allocation fails.
 */
char * xml_normalize(char * in) {
  char * out;
  int x;
  
  x = xml_normalize_length(in) +1;

  if ((out = lib_malloc(x))) {
    xml_normalize_worker(in, out, x);
  }

  return out;
}

/*
 * normalize_length:
 *    Given the input string, returns the length of that string when normalized.
 */
int xml_normalize_length(char * in) {
  int entity_len[256], size = 0, mode = 1;

  memset(entity_len, 0, sizeof(int) * 256);
  entity_len['<']  = 3; /* "&lt;" */
  entity_len['>']  = 3; /* "&gt;" */
  entity_len['"']  = 5; /* "&quot;" */
  entity_len['\''] = 5; /* "&apos;" */
  entity_len['&']  = 4; /* "&amp;" */

  while (*in) {
    if (mode==1) {
      if (('<' == *in) && (0==strncmp(in, "<![CDATA[", 9))) {
	mode = 2; 
	in += 8;
	size += 8;
      } else {
	size += entity_len[(int)*in];
      }
    } else {
      if ((']' == *in) && (0 == strncmp(in, "]]>", 3))) {
	mode = 1; 
	in += 2;
	size += 2;
      } 
    }
    size++;
    in++;
  }


  return(size);
}

/*
 * normalize_isneeded:
 *    Given the input string, returns 1 if entities requiring normalization are 
 *    found.
 */
int xml_normalize_isneeded(char * in) {  
  return(NULL != strpbrk(in, "<>'\"&"));
}
/*
 * normalize_inplace:
 *    Given the input string, this routine assumes that sufficient string buffer
 *    spaces has already been allocated to the input string and performs the 
 *    normalization in place.
 */
int xml_normalize_inplace(char * in) {
  return xml_normalize_worker(in, in, xml_normalize_length(in));
}

int xml_normalize_into(char * in, char * out) {
  return xml_normalize_worker(in, out, xml_normalize_length(in));
}

/*
 * Normalize_worker
 *
 * Does what you think it does, but backwards to support inplace work.
 */
int xml_normalize_worker(char * in, char * out, int outlen) {
  char * entities[256], * thisin, * thisout, * tmp;
  int inchar;

  memset(entities, 0, sizeof(char *) * 256);
  entities['<']  = ";tl&";   /* "&lt;"; */
  entities['>']  = ";tg&";   /* "&gt;"; */
  entities['"']  = ";touq&"; /* "&quot;"; */
  entities['\''] = ";sopa&"; /* "&apos;"; */
  entities['&']  = ";pma&";  /* "&amp;";   */

  inchar = strlen(in);
  thisin = &(in[inchar - 1]);
  thisout = &(out[outlen]);
  *thisout = '\0';
  thisout--;

  while (inchar) {
    if (entities[(int)*thisin]) {
      tmp = entities[(int)*thisin];
      while (*tmp) {
	*(thisout--) = *(tmp++);
      }
      thisout++;
    } else {
      *thisout = *thisin;
    }
    thisout--;
    thisin--;
    inchar--;
  }

  return(outlen);
}


