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
#
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <strings.h>

#include "../bp_malloc.h"
#include "../xml_entities.h"


#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif

int main () {

/*    int nbytes; */

  char * workstr, * resultstr; 
  char inplacetest[256]; 

  lib_malloc_init(malloc, free);

  printf("normalize_length: simple string: ");
  workstr = "perl uses the";
  if (strlen(workstr) == xml_normalize_length(workstr)) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("normalize_length: ");
  workstr = "perl uses the construction &lt;IN&gt; a lot.";
  if (strlen(workstr) +8 == xml_normalize_length(workstr)) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("normalize_length CDATA case: ");
  workstr = "<![CDATA[&lt;IN&gt;]]>";
  if (strlen(workstr) == xml_normalize_length(workstr)) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("normalize_isneeded: positive test: ");
  if (xml_normalize_isneeded("perl used the construction &lt;IN&gt; a lot.")) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("normalize_isneeded: ngative test: ");
  if (!xml_normalize_isneeded("perl used @_ a lot.")) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("normalize inplace: w/o entities: ");
  workstr = "king for a day";
  strcpy(inplacetest, workstr);

  if (xml_normalize_inplace(inplacetest) &&
      (!strcmp(inplacetest, workstr))) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("normalize inplace: w/entities: ");
  workstr = "'king' for a day";
  strcpy(inplacetest, workstr);
  workstr = "&apos;king&apos; for a day";

  if (xml_normalize_inplace(inplacetest) &&
      (!strcmp(inplacetest, workstr))) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  /* inclusion tests */
  printf("include_isneeded: positive: ");
  if (xml_include_isneeded("perl used the construction &lt;IN&gt; a lot.")) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("include_isneeded: negative: ");
  if (!xml_include_isneeded("perl used the construction a lot.")) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("include_length: w/entities: ");
  workstr = "perl uses the construction &lt;IN&gt; a lot.";
  if ((strlen(workstr) - 6) == xml_include_length(workstr)) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("include_length: simple: ");
  workstr = "foo";
  if (strlen(workstr) == xml_include_length(workstr)) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }
/*    if (resultstr) free(resultstr); */

  printf("include: NULL string: ");
  workstr = "";
  if ((resultstr = xml_include(workstr)) &&
      (!strcmp(workstr, resultstr))) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }
  if (resultstr) free(resultstr);

  printf("include: simple sting: ");
  workstr = "foo";
  if ((resultstr = xml_include(workstr)) &&
      (!strcmp(workstr, resultstr))) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }
  if (resultstr) free(resultstr);

  printf("include: with entities: ");
  workstr = "foo &lt;IN&gt; for a&amp;b;"; 
  if ((resultstr = xml_include(workstr)) &&
      (!strcmp("foo <IN> for a&b;", resultstr))) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }
  if (resultstr) free(resultstr);

  printf("include inplace: no work: ");
  workstr = "foo <IN> for a";
  strcpy(inplacetest, workstr);

  if (xml_include_inplace(inplacetest) &&
      (!strcmp(inplacetest, workstr))) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }


  printf("include inplace: w/entities: ");
  workstr = "foo &lt;IN&gt; for a";
  strcpy(inplacetest, workstr);
  workstr = "foo <IN> for a";

  if (xml_include_inplace(inplacetest) &&
      (!strcmp(inplacetest, workstr))) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }


  printf("include inplace: w/entities on boundaries: ");
  workstr = "&lt;&gt; foo &lt;IN&gt; for a &amp;&amp;&lt;&gt;";
  strcpy(inplacetest, workstr);
  workstr = "<> foo <IN> for a &&<>";

  printf("Boundary cases started with '%s'\n", inplacetest);

  if (xml_include_inplace(inplacetest) &&
      (!strcmp(inplacetest, workstr))) {
    printf("passed\n");
  } else {
    printf("FAILED\n");
  }

  printf("ended with '%s'\n", inplacetest);

  /*
  printf("encode test: ");
  workstr=base64_encode(rawstr, strlen(rawstr));
  if (workstr && !bcmp(workstr, codedstr, strlen(codedstr))) {
    printf("passed\n");
  } else {
    printf("FAILED'\n");
  }
  */
  exit(0);
}

