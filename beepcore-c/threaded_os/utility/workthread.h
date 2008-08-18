/*
 * Copyright (c) 2002 Huston Franklin.  All rights reserved.
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
 * $Id: workthread.h,v 1.5 2002/03/27 08:53:40 huston Exp $
 *
 * workthread.h
 *
 */

#ifndef WORKTHREAD_H
#define WORKTHREAD_H 1

#ifdef __cplusplus
extern "C" {
#endif

struct _WORKQUEUE;
typedef struct _WORKQUEUE WORKQUEUE;

WORKQUEUE* workqueue_create();
void workqueue_destroy(WORKQUEUE* queue);
int workqueue_add_item(WORKQUEUE* queue, void (*func)(void*), void* data);

#ifdef __cplusplus
}
#endif

#endif
