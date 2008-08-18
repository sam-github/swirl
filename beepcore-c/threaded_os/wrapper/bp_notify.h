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
 * $Id: bp_notify.h,v 1.5 2002/03/27 08:53:41 huston Exp $
 *
 * IW_notify.h
 *
 */
#ifndef NOTIFY_H
#define NOTIFY_H 1
#include "bp_wrapper.h"
#include "bp_wrapper_private.h"
#include "../../core/generic/CBEEP.h"

typedef struct
{
    long channel;
    int op;
    CHANNEL_INSTANCE *channel_instance;
    WRAPPER *wrap;
    struct session *s;
    int (*cbfunc)(void*);
    void *client_data;
    void (*client_data_free)(void *);
} NOTIFY_REC;

    
extern void notify_upper(struct session * s, long c, int i);

#endif    
