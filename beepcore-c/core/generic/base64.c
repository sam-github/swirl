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
 * $Id: base64.c,v 1.3 2002/03/18 21:37:35 sweetums Exp $
 *
 * base64.c
 *
 * Base64 encoding and decoding.  Uses memory allocation 
 * consistent with our usage.
 *
 */
const char * __base64_c_ver__ = "$Id: base64.c,v 1.3 2002/03/18 21:37:35 sweetums Exp $";

#include "CBEEPint.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#ifndef WIN32
#include <strings.h>
#else
#include <windows.h>
#endif

#include "base64.h"

static int vector_init = 0;
static unsigned char decode_vec[256];
static unsigned char whitespace_vec[256];
static unsigned char valid_vec[256];
static const char encode_vec[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";


/*
 * base64_esize, base64_dsize
 *
 * Utilities to compute the size of the encoded base64 string from 
 * the size of the input and the size of the payload from the 
 * encoded string.
 */
void init_vectors() {

  int i, thischar;

  if (!vector_init) {
    /* populate the whitepsce vector */
    memset(whitespace_vec, 0, 256);
    whitespace_vec['\n'] = 1;
    whitespace_vec['\t'] = 1;
    whitespace_vec['\r'] = 1;
    whitespace_vec['\f'] = 1;
    whitespace_vec[' '] = 1;

    /* populate the decoding vector */
    memset(decode_vec, 0, 256);
    for (i = 0, thischar = 'A'; 'Z' >= thischar; decode_vec[thischar++] = i++);
    for (thischar = 'a'; 'z' >= thischar; decode_vec[thischar++] = i++);
    for (thischar = '0'; '9' >= thischar; decode_vec[thischar++] = i++);
    decode_vec['+'] = i++;
    decode_vec['/'] = i++;

    /* populate valid characters vector */
    for (i = 0; 256 > i; i++) {
      if (decode_vec[i]) 
	valid_vec[i] = 1;
      if (whitespace_vec[i]) 
	valid_vec[i] = 2;
    }
    valid_vec['A'] = 1;
    valid_vec['='] = 3;

    vector_init = 1;
  }
}

/*
 * base64_esize
 *
 * Arguments:
 *    int size                  The size of the data to encode.
 * Returns:
 *    int                       Size of the base64 encoded string 
 *                              needed to carry this payload. Does not
 *                              include the null termination byte.
 */
int base64_esize(int size) {
  int x;

  x = (size / 3) * 4 + ((size % 3)?4:0);
  x += (x / BASE64_LINELEN) * 2;

  return(x);
}

/*
 * base64_dsize
 *
 * Arguments:
 *    char * encoded            The base64 encoded data.
 * Returns:
 *    int                       Size of the decoded data. 
 */
int base64_dsize(char * _encoded) {
  unsigned char* encoded = (unsigned char*) _encoded;
  int i, length, trailers;

  ASSERT(sizeof(long) * 8 >= 32);

  init_vectors();

  /* pass #1 -- validate and size */

  length = 0;
  trailers = 0;
  for (i = 0; encoded[i]; i++) {
    if (!valid_vec[(int)encoded[i]]) return(-2);
    if (1 == valid_vec[(int)encoded[i]]) length++;
    if ('=' == encoded[i]) trailers++;
  }

  if (2 < trailers || (length + trailers) % 4) return(-2);

  return(((length + trailers) / 4) * 3 - trailers);
}

/*
 * base64_encode, base64_decode
 *
 * Perform base64 manipulation on the input char* and allocate 
 * memory for the result, which is returned.  The only variance 
 * from any common library for this is that we wrap the memory 
 * managment.
 *
 * TO DO:  
 *    -   Move the lookup arrays to global const variables
 */


/*
 * base64_encode
 *
 * Arguments:
 *    session * sesion          Pinter to the session data
 *    char * raw                pointer to the source data   
 *    int size                  For _encode, the size of the data to 
 *                              encode.
 *    char * output             If NULL, allocate from session.
 *                              Otherwise, store results here.
 * Returns:
 *    char *                    Null terminated string. NULL if alloc fails.
 */
char * base64_encode_worker(struct session * session, unsigned char * raw, int size, unsigned char * output) {

  long codesize, i, x, codeidx, looplimit;
  unsigned long shifter;
  unsigned char * coded;
  ASSERT(sizeof(long) * 8 >= 32);

  /* length of coded data is 4/3 raw + CR/LF for each BASE64_LINELEN byte group + \0*/ 
  codesize = base64_esize(size) +1;

  if (NULL == output) {
    if (NULL == (coded = session->malloc(codesize))) {
      return(NULL);
    }
  } else {
    coded = output;
  }

  memset(coded, 0, codesize);

  /* encode data */
  codeidx = 0;
  shifter = 0;
  looplimit = size - (size % 3);
  for (i = 0; i < looplimit; ) {

    shifter <<= 8;
    shifter |= raw[i++];
    
    /* complete 3 raw byte group */
    if (i && 0 == (i % 3)) { 
      for (x = 3; x >= 0; x--) {
	coded[codeidx + x] = encode_vec[shifter & 0x3f];
	shifter >>= 6;
      }
      codeidx += 4;

      /* add CRLF linebreaks */
      if (!(codeidx % BASE64_LINELEN) || 
	  !((codeidx - 2) % BASE64_LINELEN)) {
	coded[codeidx++] = '\r';
	coded[codeidx++] = '\n';
      }
    }
  }
  /* last code group if needed */
  if (size % 3) {
    shifter = raw[i++];
    shifter <<= 8;
    if (size > i) {
      shifter |= raw[i++];
    }
    shifter <<= 2;

    coded[codeidx + 3] = '=';
    if (2 == (size % 3)) {
      coded[codeidx + 2] = encode_vec[shifter & 0x3f];
    } else {
      coded[codeidx + 2] = '=';
    }
    shifter >>= 6;
    coded[codeidx + 1] = encode_vec[shifter & 0x3f];
    shifter >>= 6;
    coded[codeidx] = encode_vec[shifter & 0x3f];
  }

  return (char*)(coded);
}

char * base64_encode(struct session * session, char * raw, int size) {
    return base64_encode_worker(session, (unsigned char*) raw, size, NULL);
}

char * base64_encode_into(char * raw, int size, unsigned char * output) {
    return base64_encode_worker(NULL, (unsigned char*) raw, size, output);
}

/*
 * base64_decode
 *
 * Arguments:
 *    session * sesion          Pinter to the session data
 *    char * encoded            pointer to the null terminated encoded data   
 *    char ** raw               pointer to where the stuff was allocated.
 *
 * Returns:                     N => 0 : the number of octets decoded.
 *                              N < 0 : error code
 *                                 -1      memory allocation failed
 *                                 -2      encoded data not valid base64 encoding
 *
 * Done in 2 passes to allow for allocation of memory.
 */

int base64_decode_worker(struct session * session, unsigned char * encoded, unsigned char ** raw, int mallocp) {
  long i, length, trailers, rawbyte;
  long codebyte, x;
  unsigned char * myraw;
  unsigned long shifter;


  ASSERT(sizeof(long) * 8 >= 32);
  
  init_vectors();

  /* pass #1 -- validate and size */

  length = 0;
  trailers = 0;
  for (i = 0; encoded[i]; i++) {
    if (!valid_vec[(int)encoded[i]]) return(-2);
    if (1 == valid_vec[(int)encoded[i]]) length++;
    if ('=' == encoded[i]) trailers++;
  }

  if (2 < trailers || (length + trailers) % 4) return(-2);

  if (mallocp) {
    if (NULL == (myraw = session->malloc((length / 4) * 3 + 4 - trailers))) {
      return(-1);
    }
    (*raw) = myraw;
  } else {
    myraw = *raw;
  }

  /* pass #2 -- decode */

  codebyte = 0;
  rawbyte = 0;
  shifter = 0;
  trailers = 0;
  for (i = 0; encoded[i]; i++) {
    if (whitespace_vec[(int)encoded[i]]) {
      continue;
    }
    if (!valid_vec[(int)encoded[i]]) 
      return(-2);

    if ('=' == encoded[i]) 
      trailers++;

    codebyte++;

    shifter <<= 6;
    shifter |= decode_vec[(int)encoded[i]];
    
    /* complete 4 byte group */
    if (0 == (codebyte % 4)) { 
      if (codebyte <= length) {
	for (x = 2; x > -1; x--) {
	  myraw[rawbyte + x] = (unsigned char)(shifter & 0xff);
	  shifter >>= 8;
	}
	rawbyte += 3;
      } else {
	/* should only hit for trailer>0 */
	shifter >>= 8;
	if (trailers < 2) {
	  myraw[rawbyte + 1] = (unsigned char)(shifter & 0xff);
	}
	shifter >>= 8;
	myraw[rawbyte] = (unsigned char)(shifter & 0xff);
	shifter >>= 8;
	rawbyte += 3 - trailers;
      }
    }
  }
  
  myraw[rawbyte] = 0;

  return(rawbyte);
}


int base64_decode(struct session * session, char * encoded, char ** raw) {  
  return(base64_decode_worker(session, (unsigned char*) encoded, (unsigned char**) raw, 1));
}

int base64_decode_into(struct session * session, char * encoded, char * raw) {
  return(base64_decode_worker(session, (unsigned char*) encoded, (unsigned char**) &raw, 0));
}

