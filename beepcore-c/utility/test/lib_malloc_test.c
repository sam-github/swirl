#
# Copyright (c) 2001 Invisible Worlds, Inc.  All rights reserved.
# 
# The contents of this file are subject to the Blocks Public License (the
# "License"); You may not use this file except in compliance with the License.
# 
# You may obtain a copy of the License at http://www.beepcore.org/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
# for the specific language governing rights and limitations under the
# License.
#
#
#include <stdio.h>
#include "../IW_malloc.h"


int test_malloc() {
  int i, j, x;
  char * buf;

  x = lib_malloc_init(malloc, free);

  for (i = 0; i < 10; i++) {
    buf = lib_malloc(256);
    for (j = 0; j < 256; j++) {
      buf[j] = (char)j;
    }
    lib_free(buf);
  }

  return(0);
};

int main () {
  test_malloc();

  exit(0);
}
