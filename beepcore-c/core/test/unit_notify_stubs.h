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
 * $Id: unit_notify_stubs.h,v 1.3 2001/10/30 06:00:37 brucem Exp $
 *
 * Stubs for the unit tests.
 *
 */
#include "../generic/CBEEPint.h"

void notify_lower(struct session * s, int i);
void notify_upper(struct session * s, long c, int i);

