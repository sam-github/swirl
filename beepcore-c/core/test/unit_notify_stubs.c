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
 * $Id: unit_notify_stubs.c,v 1.2 2001/10/30 06:00:37 brucem Exp $
 *
 * Stubs for the unit tests.
 *
 */
#include "CBEEPint.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void notify_lower(struct session * s, int i) {
  char * cp;
  printf("Notify lower: %d\n", i);
  cp = bll_status_text(s);
  if (cp==NULL) cp = ">>NULL<<";
  printf("  text: %s\n", cp);
  printf("  chan: %ld\n", bll_status_channel(s));
}

void notify_upper(struct session * s, long c, int i) {
  printf("Notify upper: %ld, %d\n", c, i);
}
