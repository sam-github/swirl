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

#include "../generic/CBEEP.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <strings.h>

#include "../generic/base64.h"


#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif

int main () {

  int nbytes;

  char * rawstr = "Once upon a midnight dreary as I pondered weak and weary I heard a rapping tap tap tapping at my parlor door.";
  char * codedstr = "T25jZSB1cG9uIGEgbWlkbmlnaHQgZHJlYXJ5IGFzIEkgcG9uZGVyZWQgd2VhayBhbmQgd2Vhcnkg\r\nSSBoZWFyZCBhIHJhcHBpbmcgdGFwIHRhcCB0YXBwaW5nIGF0IG15IHBhcmxvciBkb29yLg=="; 

  unsigned char * resultstr;
  struct session mysession;


  char source[] = "Hello@Example.string";
  char dest[256];
  unsigned char *foo, *bar;
  int i;
  

  mysession.malloc = malloc;
  mysession.free   = free;

  printf("encode test: ");
  resultstr = base64_encode(&mysession, rawstr, strlen(rawstr));
  if (resultstr && !bcmp(resultstr, codedstr, strlen(codedstr))) {
    free(resultstr);
    printf("passed\n");
  } else {
    printf("FAILED'\n");
  }

  printf("decode test: ");
  nbytes = base64_decode(&mysession, codedstr, &resultstr);
  if (nbytes > 0 && !bcmp(resultstr, rawstr, strlen(rawstr))) {
    free(resultstr);
    printf("passed\n");
  } else {
    printf("FAILED'\n");  }

  printf("detect length error: ");
  nbytes = base64_decode(&mysession, "AA", &resultstr);
  if (-2 == nbytes) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("detect coding error: ");
  nbytes = base64_decode(&mysession, "A}}A", &resultstr);
  if (-2 == nbytes) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("detect trailers error: ");
  nbytes = base64_decode(&mysession, "A===", &resultstr);
  if (-2 == nbytes) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  resultstr = "aa==";
  nbytes = 1;
  printf("base64_dsize: of '%s' should be %d: actually %d\n", 
	 resultstr, nbytes, base64_dsize(resultstr));

  resultstr = "aaa=";
  nbytes = 2;
  printf("base64_dsize: of '%s' should be %d: actually %d\n", 
	 resultstr, nbytes, base64_dsize(resultstr));

  resultstr = "aaaa";
  nbytes = 3;
  printf("base64_dsize: of '%s' should be %d: actually %d\n", 
	 resultstr, nbytes, base64_dsize(resultstr));

  nbytes = 1;
  printf("base64_esize: of %d should be %d: actually %d\n", 
	 nbytes, 4, base64_esize(nbytes));

  nbytes = 2;
  printf("base64_esize: of %d should be %d: actually %d\n", 
	 nbytes, 4, base64_esize(nbytes));

  nbytes = 3;
  printf("base64_esize: of %d should be %d: actually %d\n", 
	 nbytes, 4, base64_esize(nbytes));


  /*
   * incremental test and print.
   */

  memset(dest, 0, 256);

  for (i = 0; i < 10; i++) {
    dest[i] = source[i];

    foo = base64_encode(&mysession, dest, i+1);
    base64_decode(&mysession, foo, &bar);

    printf("\tinput=`%s` encoded=`%s` decoded=`%s`\n", dest, foo, bar);

    free(foo);
    free(bar);
  }

  foo = base64_encode(&mysession, rawstr, strlen(rawstr));
  base64_decode(&mysession, foo, &bar);

  printf("\tinput=`%s` encoded=`%s` decoded=`%s`\n", dest, foo, bar);

  free(foo);
  free(bar);
  

  /*
   * done-zies
   */
  exit(0);
}


