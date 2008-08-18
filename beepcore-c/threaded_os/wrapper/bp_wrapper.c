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
 * $Id: bp_wrapper.c,v 1.50 2002/04/27 04:40:22 huston Exp $
 *
 * IW_wrapper.c
 *
 * Implementation of the wrapper around the CBEEP core stuff that allows
 * it to interact sanely with the world.
 *
 */

const char _IW_wrapper_c_[]="$Id: bp_wrapper.c,v 1.50 2002/04/27 04:40:22 huston Exp $";

#ifndef WIN32
#include <unistd.h>
#include <dlfcn.h>
#include <sys/socket.h>
#endif
#include "../utility/semutex.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "bp_notify.h"
#include "bp_wrapper.h"
#include "bp_wrapper_private.h"
#include "../utility/workthread.h"
#include "../utility/logutil.h"
#include "../../utility/bp_hash.h"
#include "../../utility/bp_malloc.h"
#include "../../utility/bp_queue.h"
#include "../../utility/bp_slist_utility.h"
#include "../../core/generic/CBEEP.h"
#include "../transport/bp_fcbeeptcp.h" 
#include "../transport/bp_fiostate.h"
#include "../../core/generic/CBEEPint.h"

#define STATIC


WORKQUEUE* callback_queue = NULL;

extern void process_notify_upper(struct session* s, long c, int i);
extern void nr_callback(void* data);

#ifdef WIN32
#define vsnprintf _vsnprintf
#endif

/*
 * Local prototypes
 */
typedef int (*NOTIFYCALL)(void *);

int blw_lmem_init(WRAPPER *wrap);
void blw_lmem_cleanup(WRAPPER *wrap);

/* channel status related stuff */
void waitfor_chan_stat_answered(WRAPPER *wrap, long channel_number);

/* constructors and destructors */
/* error trace facility */
int blw_put_remembered_error(WRAPPER *wrap, long channel_number, 
                             DIAGNOSTIC *diag);
bp_slist *blw_get_remembered_error(WRAPPER *wrap, long channel_number);
int blw_remove_remembered_error(WRAPPER *wrap, long channel_number);
DIAGNOSTIC *blw_get_remembered_diagnostics(WRAPPER *wrap, long channel_number,
                                           int code, char *lang);

/* miscellany */
PROFILE_REGISTRATION * map_uri_to_reg(WRAPPER *wrap, char *uri);
int blw_wkr_post_close_confirm(void *nrvoid);
void blw_wkr_init_profile(PROFILE_INSTANCE * pro, PROFILE_REGISTRATION * reg) ;
void blwkr_greeting_dispatch(void *data);
void blw_profiles_compute_active(WRAPPER *wrap);

/*
 * Local semaphores and other important stuff.
 */
static int lib_init_complete = 0;
static sem_t sem_blw_malloc;

/*
 * Library init and cleanup routines.
 */
int bp_library_init(blw_fp_malloc newmalloc, blw_fp_free newfree)
{
    if (lib_init_complete) 
	return BLW_OK;
    else 
	lib_init_complete = 1;

    if (LIB_MALLOC_OK != lib_malloc_init(newmalloc, newfree))
	return BLW_ERROR;

    callback_queue = workqueue_create();
    if (NULL == callback_queue) {
	return BLW_ERROR;
    }
    
    SEM_INIT(&sem_blw_malloc, 1);

    return(BLW_OK);
}

int bp_library_cleanup(void) {
    workqueue_destroy(callback_queue);
    return BLW_OK;
}

/*
 *
 */
BP_CONNECTION *bp_connection_create(char *mode, IO_STATE *ios,
                                    void (*log)(int,int,char *,...),
                                    struct configobj *appconfig)
{
    WRAPPER *wrap;
    int qsize = BLW_CZ_ACTION_QSIZE;
    CHANNEL_INSTANCE * inst;

    /* $@$@$@$ We will need to enable the following code at some point. */
    /*
      if ((NULL == lib_malloc) || (NULL == lib_free))
      return(NULL);
    */

    ASSERT(callback_queue != NULL);

    wrap = (WRAPPER *)lib_malloc(sizeof(WRAPPER));
    if (!wrap)
        return NULL;
    wrap->iostate = ios;
    wrap->channel_ref_count=0;
    SEM_INIT(&wrap->lock, 1);
    SEM_INIT(&wrap->core_lock, 1);
    SEM_INIT(&wrap->callback_lock, 1);
    wrap->conn.mode = NULL;
    bp_mode_set(&wrap->conn, mode);
    wrap->session = NULL;
    wrap->log = log;
    wrap->profile_registration = NULL;
    wrap->profile_registration_active = NULL;
    wrap->remote_greeting = NULL;
    wrap->offered_profiles = NULL;    
    wrap->tuning_instance = NULL;

    /* This makes the passed in appconfig our default values, and
       in effect makes the original r/o and gives us a r/w config 
       that won't corrupt the original.
     */
    wrap->appconfig = config_new(appconfig);
    if (NULL == wrap->appconfig) {
        wrap->log(LOG_WRAP,5,"config_new: out of memory");
        return NULL;
    }


    blw_lmem_init(wrap);

    /* a channel 0 instance makes life easier for special cases. */

    inst = channel_instance_new(wrap, NULL, NULL, NULL, NULL, NULL, NULL);
    inst->channel_number = 0;
    wrap->conn.channel_instance = bp_slist__prepend(NULL, inst);;

    /* $@$@$@$ This is where qsize could get set from config */
    wrap->qsize = qsize;
    if (NULL == (wrap->pending_chan0_actions = bp_queue_new(qsize))) {
        wrap->log(LOG_WRAP,5,"bp_queue_new: out of memory");
        return NULL;
    }
    wrap->conn.status= INST_STATUS_STARTING; 
    wrap->error_remembered = bp_hash_table_init(1,BP_HASH_INT);
    if (!wrap->error_remembered) {
        bp_queue_free(wrap->pending_chan0_actions);
        return NULL;
    }

    wrap->log(LOG_WRAP, 0, "wrapper created: %d", wrap -> iostate -> socket);


    return &wrap->conn;
}

void bp_connection_destroy(BP_CONNECTION* conn)
{
    WRAPPER* wrap;
    bp_slist * tmplist;
    PROFILE_REGISTRATION *tmpreg;

    wrap = GET_WRAPPER(conn);
    if (!wrap)
        return;

    bll_session_destroy(wrap->session);
    wrap->log(LOG_WRAP, 0, "wrapper destroyed: %d", wrap -> iostate -> socket);

    /* clean up the channels. */
    while (wrap->conn.channel_instance) {
        channel_instance_destroy((CHANNEL_INSTANCE *)wrap->conn.channel_instance->data);
        wrap->conn.channel_instance =
            bp_slist__remove_link(wrap->conn.channel_instance,
                                  wrap->conn.channel_instance);
    }

    /* Delete our appconfig writable area */
    config_destroy(wrap->appconfig);

    /* clean up the registered profiles and lists. */
    tmplist = wrap->profile_registration;
    while (tmplist)
    {
        tmpreg = (PROFILE_REGISTRATION *)tmplist->data;
        tmpreg->proreg_session_fin(tmpreg, &wrap->conn);
        tmpreg->proreg_connection_fin(tmpreg, &wrap->conn);
        bp_profile_registration_destroy(&wrap->conn, tmpreg);
        tmplist=tmplist->next;
    }

    bp_slist__free(wrap->profile_registration);
    bp_slist__free(wrap->profile_registration_active);

    if (wrap->offered_profiles)
        lib_free(wrap->offered_profiles);

    if (wrap->remote_greeting)
        lib_free(wrap->remote_greeting);

    bp_queue_free(wrap->pending_chan0_actions);
    bp_hash_table_fin(&wrap->error_remembered);
    bp_slist__free(wrap->tuning_instance);
    wrap->tuning_instance=NULL;

    /* clean up memory */
#if 0
    /** @todo this causes an occasional page fault */
    blw_lmem_cleanup(wrap);
#endif

    lib_free(wrap->conn.mode);

    wrap->conn.status = INST_STATUS_EXIT;
    fiostate_stop(wrap->iostate);

    /** @todo implement a proper fix instead of this patch */
    while (wrap->iostate != NULL) {
        YIELD();
    }

    lib_free(wrap);

    return;
}

/*
 * blw_profile_register
 *
 * 
 * Returns:
 *     NULL            For successful registration.
 *     DIAGNOSTIC *    If there was an error either in the wrapper or
 *                     as returned by the profile init routine.  This
 *                     DIAGNOSTIC * should be freed using the library 
 *                     routine blw_diagnostic_destroy.
 */
DIAGNOSTIC* bp_profile_register(BP_CONNECTION* conn, PROFILE_REGISTRATION* reg)
{
    WRAPPER* wrap = GET_WRAPPER(conn);
    char * str;
    bp_slist * tmplist;
    PROFILE_REGISTRATION * myreg;
    DIAGNOSTIC * diag;

    /* Rough validation */
    if ((NULL == reg->uri) || 
        (NULL == reg->initiator_modes) || 
        (NULL == reg->listener_modes) ||
        (NULL == reg->proreg_start_indication) ||
        (NULL == reg->proreg_start_confirmation) ||
        (NULL == reg->proreg_close_indication) ||
        (NULL == reg->proreg_close_confirmation) ||
        (NULL == reg->proreg_tuning_reset_indication) ||
        (NULL == reg->proreg_tuning_reset_confirmation) ||
        (reg->full_messages && (NULL == reg->proreg_message_available)) ||
        (!reg->full_messages && (NULL == reg->proreg_frame_available)))
    {
      return(blw_diagnostic_new(wrap, BLW_REGISTER_INCOMPLETE, NULL,
                                "registration structure incomplete"));
    }

    /* Check against the currently registered for a duplicate uri */
    for (tmplist = wrap->profile_registration; 
         NULL != tmplist; tmplist = tmplist->next)
    {
        myreg = (PROFILE_REGISTRATION *)tmplist->data;

        if (0 == strcmp(myreg->uri, reg->uri)) {
            return(blw_diagnostic_new(wrap, BLW_REGISTER_DUPURI, NULL,
                                      "URI already registered: %s", reg->uri));
        }
    }

    /* OK we passed.  Dup the struct (single allocation) and add to the list */

    myreg = bp_profile_registration_clone(&wrap->conn, reg);
    if (NULL == myreg) {
        return(blw_diagnostic_new(wrap, BLW_ALLOC_FAIL, NULL,
                                  "bp_profile_registration_clone: out of memory"));
    } else {
        /* need to do the pro_connection_init and verify it */
        str = myreg->proreg_connection_init(myreg, &wrap->conn);
        if (NULL == str) {
            wrap->profile_registration =
                bp_slist__append(wrap->profile_registration, myreg);
            if (strstr(myreg->initiator_modes, wrap->conn.mode) || 
                strstr(myreg->listener_modes, wrap->conn.mode))
            {
                wrap->profile_registration_active =
                    bp_slist__append(wrap->profile_registration_active, myreg);
            }
        } else {
            lib_free(myreg);
            diag = blw_diagnostic_new(wrap, BLW_REGISTER_INITFAIL, NULL,
                                      "%s connection init failed: %s",
                                      reg -> uri, str);
            blw_free(wrap, str);
            return(diag);
        }
    }
    return(NULL);
}

void blw_profiles_compute_active(WRAPPER *wrap) 
{
    bp_slist * tmp;
    PROFILE_REGISTRATION * myreg;

    bp_slist__free(wrap->profile_registration_active);
    wrap->profile_registration_active = NULL;

    tmp = wrap->profile_registration;
    while (tmp) {
        myreg = (PROFILE_REGISTRATION *)tmp->data;
        if (myreg) {
            if (strstr(myreg->initiator_modes, wrap->conn.mode) || 
                strstr(myreg->listener_modes, wrap->conn.mode))
            {
                myreg->status = 'I';
                wrap->profile_registration_active =
                    bp_slist__append(wrap->profile_registration_active, myreg);
            } else {
                myreg->status = (char)0;
            }
        }
        tmp = tmp->next;
    }

    return;
}

/*
 *
 */
void blw_greeting_notification(WRAPPER *wrap, struct chan0_msg *notify)
{
    int pc; /* profile count */
    struct profile * p;

    int err;

    SEM_WAIT(&wrap->lock);

    /* the basics, save it */
    wrap->remote_greeting = notify;

    for (pc = 0, p = notify->profile; p != NULL; pc += 1, p = p->next)
        /* Just count */;
    wrap->offered_profiles = (char**)(lib_malloc((pc+1)*sizeof(char **)));

    if (wrap->offered_profiles == NULL) {
        wrap->log(LOG_WRAP, 5, "blw_greeting_notification: out of memory");
        /* $@$@$@$ Something dire should happen here. */
    } 
    else {
        for (pc = 0, p = notify->profile; p != NULL; pc += 1, p = p->next) 
            wrap->offered_profiles[pc] = p->uri;
        wrap->offered_profiles[pc] = NULL;
    }
    /* set our state */
    wrap->conn.status = INST_STATUS_RUNNING;

    /* notify on the greeting, we do this in another thread in case someone
       does something that will block chan0 thread 
    */

    err = workqueue_add_item(callback_queue, blwkr_greeting_dispatch, wrap);

    SEM_POST(&wrap->lock);

    /* @todo shutdown session on error */
    ASSERT(err == 0);
}

void blwkr_greeting_dispatch(void *data) {
    WRAPPER *wrap = (WRAPPER*) data;

    PROFILE_REGISTRATION *preg;
    bp_slist *tmp;
    char **tmpuri;
    char pflag;
    struct chan0_msg *confirm;
    
    confirm = wrap->remote_greeting;

    for (tmp = wrap->profile_registration_active; tmp; tmp = tmp->next) {
        preg = tmp->data;
        if (preg) {
            if (confirm->error) {
                pflag = GREETING_ERROR;
            } else {
                pflag = PROFILE_ABSENT;
                for (tmpuri = wrap->offered_profiles; *tmpuri; tmpuri++) {
                    if (0 == strcmp(*tmpuri, preg->uri)) {
                        pflag = PROFILE_PRESENT;
                        break;
                    }
                }
            }
            if (preg->proreg_greeting_notification) {
                preg->proreg_greeting_notification(preg, &wrap->conn, 'P');
            }
        }
    }
}

/*
 *
 */
DIAGNOSTIC* bp_start_request(BP_CONNECTION* conn, long channel_number, 
                             long message_number, struct profile* profile, 
                             char* server_name, 
                             start_request_callback app_sr_callback,
                             void* client_data)
{
    WRAPPER* wrap = GET_WRAPPER(conn);
    struct chan0_msg *request;
    bp_slist * list;
    CHANNEL_INSTANCE * inst;
    struct profile * pro;
    DIAGNOSTIC *diag;
    long chan;


    request = blw_chan0_msg_new(wrap, channel_number, message_number,
                                'i', 's', profile, NULL, "", "", server_name);

    /* 
     * $@$@$@$ This could be done in one pass if we stored the uri's in 
     * a hash table.
     */
    SEM_WAIT(&wrap->lock);

    pro = request->profile;
    while (pro) {
        list = wrap->profile_registration_active;
        if (wrap->conn.status == INST_STATUS_EXIT)
            list = NULL;
        while (list) {
            if (0 == strcmp(pro->uri, ((PROFILE_REGISTRATION*)list->data)->uri))
                break;
            list = list->next;
        }
        if (NULL == list)
        {
            diag = blw_diagnostic_new(wrap, BLW_START_PROF_NOT_REG, NULL,
                                      "URI not registered: %s", pro->uri);
            blw_chan0_msg_destroy(wrap, request);
            SEM_POST(&wrap->lock);
            return diag;
        }
        pro = pro->next;
    }

    /* We must complete the queueing of the instance before other blw_
     * can run
     */

    /* $@$@$@$ If this fails we need to do something drastic, it calls
     * fault.
     */
    inst = channel_instance_new(wrap, NULL, NULL, request, app_sr_callback, 
                                NULL, client_data);
    if (inst == NULL) {
        /* $@$@$@$ need to FAULT here? */
        blw_chan0_msg_destroy(wrap, request);
        SEM_POST(&wrap->lock);
        return(blw_diagnostic_new(wrap, BLW_ALLOC_FAIL, "",
                                  "Memory allocation failed"));
    }
    inst->status = INST_STATUS_STARTING;

/*  prepend here in case of blw_shutdown */

    wrap->conn.channel_instance =
        bp_slist__prepend(wrap->conn.channel_instance, inst); 

    SEM_POST(&wrap->lock);

    SEM_WAIT(&wrap->core_lock);
    if (-1 == (chan = blu_channel_start(wrap->session, request))) {
        SEM_POST(&wrap->core_lock);
        SEM_WAIT(&wrap->lock);
        diag = blw_get_remembered_diagnostics(wrap,request->channel_number,
                                              BLW_BEEP_LIBRARY_ERROR,NULL);
        blw_chan0_msg_destroy(wrap, request);

        wrap->conn.channel_instance =
            bp_slist__remove(wrap->conn.channel_instance, inst);
        SEM_POST(&wrap->lock);
        
        lib_free(inst);
        if (!diag) {
            return(blw_diagnostic_new(wrap, BLW_BEEP_LIBRARY_ERROR, NULL, 
                                      "blu_channel_start failed"));
        } else {
            return diag;
        }
    } else {
        SEM_POST(&wrap->core_lock);
    }

    inst->channel_number = chan;
    SEM_WAIT(&wrap->core_lock);
    blu_channel_info_set(wrap->session, inst->channel_number, inst);
    SEM_POST(&wrap->core_lock);

    SEM_POST(&wrap->lock);
    if (bp_queue_put_and_grow(wrap->pending_chan0_actions, inst, wrap->qsize)) {
        blw_chan0_msg_destroy(wrap, request);
        wrap->conn.channel_instance =
            bp_slist__remove(wrap->conn.channel_instance, inst);
        SEM_POST(&wrap->lock);
        lib_free(inst);
        /* @todo this should close either the channel or the session */
        return(blw_diagnostic_new(wrap, BLW_ALLOC_FAIL, NULL,
                                  "bp_queue_grow: out of memory"));
    }

    if (wrap->conn.status != INST_STATUS_EXIT) {
        SEM_POST(&wrap->lock);
        SEM_WAIT(&inst->user_sem1);
    } else {
        SEM_POST(&wrap->lock);
    }

    return NULL;
}

/*
 *
 */
void blw_start_confirmation(WRAPPER *wrap, struct chan0_msg *confirm)
{
    bp_slist * list;
    CHANNEL_INSTANCE * inst;
    DIAGNOSTIC *diag;

    /* $@$@$@$ Error checking here? The library should insure the
     * reply sequence
     */

    SEM_WAIT(&wrap->lock);
    inst = bp_queue_get(wrap->pending_chan0_actions);

    if (confirm->error) {
        /* was in blw_action_error */
        /* IS THIS RIGHT TO NOT INCLUDE THE CHANNEL INSTANCE? I can see why it
        would be good not to send it, but how do they know which channel didn't start? */
        SEM_POST(&wrap->lock);

        if (inst -> app_sr_callback) {
            SEM_WAIT(&wrap->callback_lock);
            inst->app_sr_callback(inst->cb_client_data, NULL, confirm->error);
            SEM_POST(&wrap->callback_lock);
        }
        if (inst->outbound)
            blw_chan0_msg_destroy(wrap, inst->outbound);
        blw_chan0_msg_destroy(wrap, confirm);
        SEM_POST(&inst->user_sem1);
        SEM_WAIT(&wrap->lock);
        wrap->conn.channel_instance =
            bp_slist__remove(wrap->conn.channel_instance, inst);
        channel_instance_destroy(inst);
        SEM_POST(&wrap->lock);
        return;
    }

    list = wrap->profile_registration_active;
    while (list) {
        if (0 == strcmp(confirm->profile->uri, ((PROFILE_REGISTRATION*)list->data)->uri))
            break;
        list = list->next;
    }
    /* $@$@$@$ NULL == list is going to be session fatal?  not sure. */
    /* check to see if diag should be traped for indication function */
    if (NULL == list) {
        diag = blw_diagnostic_new(wrap,550,NULL,"URI not supported: %s",
                                  confirm->profile->uri);
        blw_put_remembered_error(wrap,inst->channel_number,diag);
        SEM_POST(&wrap->lock);
        SEM_POST(&inst->user_sem1);
        /* @todo close the channel or session here */
        return;
        /*
        printf("Fear, Fire, Foes, Awake!!!!");
        exit(1);    */

    }

    inst->profile_registration = (PROFILE_REGISTRATION*)list->data;

    SEM_POST(&wrap->lock);

    blw_wkr_init_profile(inst->profile, inst->profile_registration); 

    inst->status = INST_STATUS_RUNNING;

    SEM_WAIT(&wrap->callback_lock);
    inst->profile->pro_start_confirmation(inst->cb_client_data,
                                          inst->profile, 
                                          confirm->profile);

    if (inst->app_sr_callback)
        inst->app_sr_callback(inst->cb_client_data, inst, NULL);

    SEM_POST(&wrap->callback_lock);

    SEM_POST(&inst->user_sem1); 

    SEM_WAIT(&wrap->lock); 
    blw_chan0_msg_destroy(wrap, inst->outbound);
    blw_chan0_msg_destroy(wrap, confirm);

    inst->inbound = NULL;
    inst->outbound = NULL;

    SEM_POST(&wrap->lock);

    /* drain pending events */
    for (;;) {
        PENDING_EVENT* event = bp_queue_get(inst->pending_event);
        if (event == NULL)
            break;

        process_notify_upper(event->s, event->c, event->i);

        lib_free(event);
    }
    return;
}

void blw_start_indication(WRAPPER *wrap, struct chan0_msg *request)
{
    CHANNEL_INSTANCE * inst;
    PROFILE_REGISTRATION * myreg;
    struct profile * pro /* , protmp */;
    DIAGNOSTIC * nojoy = NULL;

    /* get an instance */

    SEM_WAIT(&wrap->lock);
    if (NULL == (inst = channel_instance_new(wrap, NULL, request, NULL, NULL, 
                 NULL, NULL)))
    {
        wrap->log(LOG_WRAP,5,"channel_instance_new: out of memory");
        SEM_POST(&wrap->lock);
        /* @todo should the session be closed? */
        return;
    }
	
    
    /* 
     * $@$@$@$ This could be done in one pass if we stored the uri's in 
     * a hash table.
     */
    pro = request->profile;

    while (pro) {
        if (NULL != (myreg = map_uri_to_reg(wrap, pro->uri))) {

            /* Got a mapping.  try for a start */
            inst->profile_registration = myreg;
            inst->status = INST_STATUS_STARTING;
            inst->diagnostic = NULL;
            inst->outbound = NULL;

            blw_wkr_init_profile(inst->profile, myreg);
            SEM_POST(&wrap->lock);
            SEM_WAIT(&wrap->callback_lock);
            myreg->proreg_start_indication(inst->profile, pro);
            SEM_POST(&wrap->callback_lock);
            SEM_WAIT(&wrap->lock);
            // if there was an error set, then send it
            if (inst->diagnostic || inst->outbound) {
                break;
            }
        } else {
            if (!nojoy) {
                nojoy = blw_diagnostic_new(wrap, 550, NULL,
                                           "no requested profiles are supported");
            }
        }
        pro = pro->next;
    }


    if (pro && (inst->outbound || inst->diagnostic)) {
        wrap->conn.channel_instance =
          bp_slist__prepend(wrap->conn.channel_instance, inst);

        PRE(inst->diagnostic == NULL || inst->outbound == NULL);

        if (inst->diagnostic) {
            blw_chan0_reply(wrap, request, NULL, inst->diagnostic);
            SEM_POST(&wrap->lock);
        }
        else if (inst->outbound) {

            blw_chan0_reply(wrap, request, inst->outbound->profile, NULL);
            blw_chan0_msg_destroy(wrap, inst->outbound);
            inst->outbound = NULL;

            for (;;) {
                struct frame* f = bp_queue_get(inst->pending_send);
                if (f == NULL) {
                    break;
                }
                blu_frame_send(f);
            }

            inst->status = INST_STATUS_RUNNING;
            SEM_POST(&wrap->lock);
            YIELD();

            /* $@$@$@$ OK we need to do something different here. I think we 
             * need to actually catch that we are in a tuning reset from the
             * blw_tuning reset call. 
             */
            SEM_WAIT(&wrap->core_lock);
            blu_channel_info_set(wrap->session, inst->channel_number, inst);
            SEM_POST(&wrap->core_lock);
        }

        return;
    }

    /* No profiles started, Send the saved error  */
    nojoy = blw_diagnostic_new(wrap, 550, NULL,
                               "no requested profiles are supported");
    blw_chan0_reply(wrap, request, NULL, nojoy);
    blw_diagnostic_destroy(wrap, nojoy);
    if (inst)
        channel_instance_destroy(inst);

    SEM_POST(&wrap->lock);

    return;
}

DIAGNOSTIC * blw_close_request_0(WRAPPER *wrap, struct chan0_msg *request,
                                 close_request_callback app_cr_callback,
                                 void *client_data)
{
    bp_slist * list, * rblist = NULL;
    CHANNEL_INSTANCE * inst;
    DIAGNOSTIC * mydiag = NULL;
    PROFILE_INSTANCE * mypro;
	
    /* construct our action list */
    SEM_WAIT(&wrap->lock);

    /* closing down the session */
    if (wrap->pending_chan0_actions->entries) {
        mydiag = blw_diagnostic_new(wrap, BLW_CHANNEL_BUSY, NULL,
            "pending action(s) on channel zero");
        ASSERT(mydiag != NULL);

        blw_chan0_msg_destroy(wrap, request);
        SEM_POST(&wrap->lock);

        return mydiag;
    }
    
    inst = blw_map_channel_number(wrap, 0);
    ASSERT(inst != NULL);
    inst->status = INST_STATUS_CLOSING;
    inst->outbound = request;
    inst->inbound = NULL;
    inst->app_cr_callback = app_cr_callback;
    inst->cb_client_data = client_data;
    
    /* tell em all */
    list = wrap->conn.channel_instance;
    while (list) {
        inst = (CHANNEL_INSTANCE *)list->data;
        ASSERT(inst != NULL);
        if (inst->channel_number != 0) {
            mydiag = blw_wkr_cr_instance(wrap, inst, request,
                                         app_cr_callback,
                                         PRO_ACTION_LOCAL);
            if (mydiag) {
                break;
            }
            rblist = bp_slist__prepend(rblist, inst);
        }
        
        list = list->next;
    }

    /* queue this action */
    if (NULL == mydiag) {
        /* might have to grow the queue, probably not. */
        if (bp_queue_put_and_grow(wrap->pending_chan0_actions, inst,
                                  wrap->qsize))
        {
            /* $@$@$@$ might just need to fault here */		
            mydiag = blw_diagnostic_new(wrap, BLW_ALLOC_FAIL, NULL,
                                        "memory allocation failed");
        }
    } 

    if (mydiag) {
        /* roll em back if needed */
        inst->status = INST_STATUS_RUNNING;	    
        list = rblist;
        while (list) {
            inst = (CHANNEL_INSTANCE *)list->data;
            inst->status = INST_STATUS_RUNNING;
            if (inst->channel_number) {
                mypro = inst->profile;
                SEM_WAIT(&wrap->callback_lock);
                mypro->pro_close_confirmation(inst->profile,
                                              PRO_ACTION_FAILURE, NULL,
                                              PRO_ACTION_LOCAL,
                                              PRO_ACTION_CHANNEL);
                SEM_POST(&wrap->callback_lock);
            }
            list = list->next;
        }

        blw_chan0_msg_destroy(wrap, request);
        if (rblist) 
            bp_slist__free(rblist);

        SEM_POST(&wrap->lock);

        return mydiag;
    }

    /* send the close */
    if (wrap->conn.status != INST_STATUS_EXIT) {
        waitfor_chan_stat_answered(wrap, request->channel_number);
    }

    if (wrap->conn.status == INST_STATUS_EXIT) {
        SEM_POST(&wrap->lock);

        bp_queue_get(wrap->pending_chan0_actions);

        list = wrap->conn.channel_instance;
        while (list) {
            CHANNEL_INSTANCE* inst2 = (CHANNEL_INSTANCE *)list->data;	    
            if (inst2 && inst2->channel_number) {
                mypro = inst2->profile;
                SEM_WAIT(&wrap->callback_lock);
                mypro->pro_close_confirmation(inst2->profile,
                                              PRO_ACTION_SUCCESS,
                                              NULL,
                                              PRO_ACTION_LOCAL,
                                              PRO_ACTION_CHANNEL);
                SEM_POST(&wrap->callback_lock);
                SEM_WAIT(&wrap->lock);
                wrap->conn.channel_instance =
                    bp_slist__remove_link(wrap->conn.channel_instance,
                                          wrap->conn.channel_instance);
                channel_instance_destroy(inst2);
                list = wrap->conn.channel_instance;
                SEM_POST(&wrap->lock);
            } else {
                list = list->next;
            }
        }
        if (inst->app_cr_callback)
            inst->app_cr_callback(inst->cb_client_data, inst, NULL);

        SEM_WAIT(&wrap->lock);
        SEM_WAIT(&wrap->core_lock);
        blu_channel_close(wrap->session, request->channel_number,
                          request->error);
        SEM_POST(&wrap->core_lock);
        blw_chan0_msg_destroy(wrap, request);
    } else {
        SEM_WAIT(&wrap->core_lock);
        blu_channel_close(wrap->session, request->channel_number, 
            request->error);
        SEM_POST(&wrap->core_lock);
    }

    if (wrap->conn.status == INST_STATUS_EXIT) {
        list = rblist;
        while (list) {
            inst = (CHANNEL_INSTANCE *)list->data;
            if (inst)
                mydiag = blw_get_remembered_diagnostics(wrap,
                                                        inst->channel_number,
                                                        200,NULL);
            if (mydiag)
                list=NULL;
            else
                list=list->next;
        }
    }

    SEM_POST(&wrap->lock);

    if (rblist) 
        bp_slist__free(rblist);

    return mydiag;
}

DIAGNOSTIC * blw_close_request_non0(WRAPPER *wrap, struct chan0_msg *request,
                                    close_request_callback app_cr_callback,
                                    void *client_data)
{
    CHANNEL_INSTANCE * inst;
    DIAGNOSTIC * mydiag = NULL;
    PROFILE_INSTANCE * mypro;
	
    /* construct our action list */
    SEM_WAIT(&wrap->lock);

    /* closing the channel */
    if (NULL == (inst = blw_map_channel_number(wrap, request->channel_number))) {
        mydiag = blw_diagnostic_new(wrap, BLW_CHANNEL_NOT_OPEN, NULL,
                                    "channel not open: %d",
                                    request->channel_number);
        ASSERT(mydiag != NULL);

        blw_chan0_msg_destroy(wrap, request);
        SEM_POST(&wrap->lock);

        return mydiag;
    }

    if (INST_STATUS_RUNNING == inst->status) {
        mydiag = blw_wkr_cr_instance(wrap, inst, request,
                                     app_cr_callback, PRO_ACTION_LOCAL);
    } else {
        mydiag = blw_diagnostic_new(wrap, BLW_CHANNEL_BUSY,
                                    NULL,
                                    "pending action(s) on channel: status=%c",
                                    inst -> status);
    }

    /* queue this action */
    if (NULL == mydiag) {
        /* might have to grow the queue, probably not. */
        if (bp_queue_put_and_grow(wrap->pending_chan0_actions, inst,
                                  wrap->qsize))
        {
            /* $@$@$@$ might just need to fault here */		
            mydiag = blw_diagnostic_new(wrap, BLW_ALLOC_FAIL, NULL,
                                        "memory allocation failed");
        }
    } 

    if (mydiag) {
        /* roll em back if needed */
        blw_chan0_msg_destroy(wrap, request);
        SEM_POST(&wrap->lock);

        mypro = inst->profile;
        if (mypro->pro_close_confirmation) {
            SEM_WAIT(&wrap->callback_lock);
            mypro->pro_close_confirmation(inst->profile,
                                          PRO_ACTION_FAILURE, 
                                          request->error,
                                          PRO_ACTION_LOCAL,
                                          PRO_ACTION_CHANNEL);
            SEM_POST(&wrap->callback_lock);
        }

        return mydiag;
    }

    /* send the close */
    if (wrap->conn.status != INST_STATUS_EXIT) {
        waitfor_chan_stat_answered(wrap, request->channel_number);
    }

    if (wrap->conn.status == INST_STATUS_EXIT) {
        SEM_POST(&wrap->lock);
        /* Cleanup */
        bp_queue_get(wrap->pending_chan0_actions);

        waitfor_chan_stat_quiescent(wrap, inst->outbound->channel_number);

        mypro = inst->profile;
        mypro->pro_close_confirmation(inst->profile, PRO_ACTION_SUCCESS,
                                      NULL,
                                      PRO_ACTION_LOCAL,
                                      PRO_ACTION_CHANNEL);
        SEM_WAIT(&wrap->lock);
        wrap->conn.channel_instance =
            bp_slist__remove(wrap->conn.channel_instance, inst);
        channel_instance_destroy(inst);
        SEM_POST(&wrap->lock);
        if (inst->app_cr_callback)
            inst->app_cr_callback(inst->cb_client_data, inst, NULL);

        SEM_WAIT(&wrap->core_lock);
        blu_channel_close(wrap->session, request->channel_number,
                          request->error);
        SEM_POST(&wrap->core_lock);

        blw_chan0_msg_destroy(wrap, request);
    } else {
        SEM_POST(&wrap->lock);

        SEM_WAIT(&wrap->core_lock);
        blu_channel_close(wrap->session, request->channel_number, 
            request->error);
        SEM_POST(&wrap->core_lock);
    }

    return mydiag;
}

DIAGNOSTIC * blw_close_request_prim(WRAPPER *wrap, struct chan0_msg *request,
                                    close_request_callback app_cr_callback,
                                    void *client_data)
{
    if (request->channel_number == 0) {
        return blw_close_request_0(wrap, request, app_cr_callback,
                                   client_data);
    } else{
        return blw_close_request_non0(wrap, request, app_cr_callback,
                                      client_data);
    }
}

DIAGNOSTIC* bpc_close_request(CHANNEL_INSTANCE* channel,
                              long message_number, long result, char* language,
                              char* message, 
                              close_request_callback app_cr_callback, 
                              void* client_data)
{
    WRAPPER* wrap = GET_WRAPPER(channel->conn);
    long channel_number = channel->channel_number;
    DIAGNOSTIC srcdiag;
    struct chan0_msg *request;

    srcdiag.code = result;
    srcdiag.lang = language;
    srcdiag.message = message;

    request = blw_chan0_msg_new(wrap, channel_number, message_number, 'i' ,
                                'c',  NULL, &srcdiag, "", "", NULL );

    return blw_close_request_prim(wrap, request, app_cr_callback, client_data);
}

void blw_chan0_reply(WRAPPER *wrap, struct chan0_msg *c0m, struct profile *pro,
                     DIAGNOSTIC *diag)
{
    SEM_WAIT(&wrap->core_lock);
    blu_chan0_reply(c0m,pro,diag);
    SEM_POST(&wrap->core_lock);
    return;
}

/*
 * blw_wkr_cr_instance
 * 
 * Local worker funciton to actually call the instance close_request
 */
DIAGNOSTIC * blw_wkr_cr_instance(WRAPPER *wrap, CHANNEL_INSTANCE * inst,
				 struct chan0_msg *request,
				 close_request_callback app_cr_callback,
				 char local)
{
    PROFILE_INSTANCE * mypro;
    char sesschan;

    /* closing the channel */
    sesschan = PRO_ACTION_CHANNEL;

    mypro = inst->profile;

    inst->status = INST_STATUS_CLOSING;
    inst->outbound = request;
    inst->inbound = NULL;
    inst->app_cr_callback = app_cr_callback;

    /* 
     * $@$@$@$ at the moment this is serial and not calling 
     * in another thread, so there are no sync issues.
     */
    /* send the indications locally, causes inst->diag to be set  */
    inst->diagnostic = NULL;
    mypro->pro_close_indication(inst->profile, request->error, local,
                                sesschan);

    /* check for negative response */
    if (inst->diagnostic) {
        inst->status = INST_STATUS_RUNNING;
        mypro->pro_close_confirmation(inst->profile, PRO_ACTION_FAILURE,
                                      inst->diagnostic, local, sesschan);
        if (PRO_ACTION_LOCAL == local && app_cr_callback) {
            app_cr_callback(inst->cb_client_data, inst, inst->diagnostic);
        }
    }
    
    return(inst->diagnostic);
}

void blw_wkr_init_profile(PROFILE_INSTANCE * pro, PROFILE_REGISTRATION * reg) 
{
    if (reg) {
        pro->full_messages      = reg->full_messages;

        pro->pro_start_indication   = reg->proreg_start_indication;
        pro->pro_start_confirmation = reg->proreg_start_confirmation;
        pro->pro_close_indication   = reg->proreg_close_indication;
        pro->pro_close_confirmation = reg->proreg_close_confirmation;
        pro->pro_tuning_reset_indication = reg->proreg_tuning_reset_indication;
        pro->pro_tuning_reset_confirmation =
            reg->proreg_tuning_reset_confirmation;
        pro->pro_frame_available    = reg->proreg_frame_available;
        pro->pro_message_available  = reg->proreg_message_available;
        pro->pro_window_full        = reg->proreg_window_full;
    }

    pro->user_long1=0;
    pro->user_long2=0;
    pro->user_ptr1=NULL;
    pro->user_ptr2=NULL;

    return;
}

/*
 * PRE: acquire wrap->lock
 */
CHANNEL_INSTANCE *blw_map_channel_number(WRAPPER *wrap, long channel_number)
{
    bp_slist * list;

    list = wrap->conn.channel_instance;
    while (list) {
        if (channel_number == ((CHANNEL_INSTANCE *)list->data)->channel_number) {
            SEM_POST(&wrap->lock);
            return ((CHANNEL_INSTANCE *)list->data);
        }
        list = list->next;
    }
    return NULL;
}

char* bp_server_name(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    if (wrap && wrap->remote_greeting) {
        return wrap->remote_greeting->server_name;
    }

    return NULL;
}

char* bp_server_localize(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    if (wrap && wrap->remote_greeting) {
        return wrap->remote_greeting->localize;
    }

    return NULL;
}

char* bp_features(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    if (wrap && wrap->remote_greeting) {
        return wrap->remote_greeting->features;
    }

    return NULL;
}

char** bp_profiles_offered(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    if (wrap)
        return wrap->offered_profiles;
    else 
        return NULL;
}

struct diagnostic* bp_greeting_error(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    if (wrap && wrap->remote_greeting) {
        return wrap->remote_greeting->error;
    }
    return NULL;
}

char** bp_profiles_avail_listen(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    int pcount;
    char **ret = NULL, **this;
    bp_slist * item;
    PROFILE_REGISTRATION * prof;

    SEM_WAIT(&wrap->lock);
    pcount = bp_slist__length(wrap->profile_registration_active) + 1;
    if (NULL != (ret = lib_malloc(pcount * sizeof(char*)))) {
        this = ret;
        item = wrap->profile_registration_active;
        while (NULL != item) {
            prof = (PROFILE_REGISTRATION*)(item->data);
            if (NULL != strstr(prof->listener_modes, wrap->conn.mode)) {
                *(this++) = prof->uri;
                prof->status = 'L';
            }
            item = item->next;
        }
        *this = NULL;
    }

    SEM_POST(&wrap->lock);
    return ret;
}

char** bp_profiles_avail_initiate(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    int pcount;
    char **ret = NULL, **this;
    bp_slist * item;
    PROFILE_REGISTRATION * prof;

    SEM_WAIT(&wrap->lock);
    pcount = bp_slist__length(wrap->profile_registration_active) + 1;
    if (NULL != (ret = lib_malloc(pcount * sizeof(char*)))) {
        this = ret;
        item = wrap->profile_registration_active;
        while (NULL != item) {
            prof = (PROFILE_REGISTRATION*)(item->data);
            if (NULL != strstr(prof->initiator_modes, wrap->conn.mode)) {
                *(this++) = prof->uri;
            }
            item = item->next;
        }
        *this = NULL;
    }
    SEM_POST(&wrap->lock);

    return ret;
}

char** bp_profiles_registered(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    int pcount;
    char **ret = NULL, **this;
    bp_slist * item;

    SEM_WAIT(&wrap->lock);
    pcount = bp_slist__length(wrap->profile_registration) + 1;
    if (NULL != (ret = lib_malloc(pcount * sizeof(char*)))) {
        this = ret;
        item = wrap->profile_registration; 
        while (NULL != item) {
            *(this++) = ((PROFILE_REGISTRATION*)(item->data))->uri;
            item = item->next;
        }
        *this = NULL;
    }
    SEM_POST(&wrap->lock);
    return ret;
}

DIAGNOSTIC* bp_wait_for_greeting(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);
    int status = wrap -> conn.status;

    switch (status) {
        case INST_STATUS_STARTING:
        case INST_STATUS_TUNING:
            while (wrap -> conn.status == status)
                YIELD ();
            break;
    }
    
    return bp_greeting_error(&wrap->conn);
}




char **blw_pro_session_init(WRAPPER *wrap)
{
    char * retstr;
    bp_slist * item, * tmp;
    PROFILE_REGISTRATION * myreg;

    SEM_WAIT(&wrap->lock);

    blw_profiles_compute_active(wrap);

    item = wrap->profile_registration_active; 
    while (NULL != item) {
        myreg = (PROFILE_REGISTRATION*)item->data; 
        retstr = myreg->proreg_session_init(myreg, &wrap->conn);

        tmp = item;
        item = item->next;
        if (NULL != retstr) {
            /* $@$@$@$ what? no logging yet!  bad toad.  */
            wrap->profile_registration_active =
                bp_slist__remove(wrap->profile_registration_active, myreg);
        }
    }
    SEM_POST(&wrap->lock);
    return(bp_profiles_avail_listen(&wrap->conn));
}

void bp_mode_set(BP_CONNECTION* conn, char* mode)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    if (wrap->conn.mode)
      lib_free(wrap->conn.mode);

    if (!mode) {
      mode = "";
    }
    wrap->conn.mode = (char *)lib_malloc(strlen(mode)+1);
    sprintf(wrap->conn.mode, "%s", mode);

    return;
}

void bp_set_session_info(BP_CONNECTION* conn, void* user_info)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    SEM_WAIT(&wrap->lock);
    wrap->session_info = user_info;
    SEM_POST(&wrap->lock);
    return;
}

void* bp_get_session_info(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    void *ret;

    SEM_WAIT(&wrap->lock);
    ret = wrap->session_info;
    SEM_POST(&wrap->lock);
    return ret;
}


struct configobj* bp_get_config(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    return wrap->appconfig;
}

int blw_channel_status(WRAPPER *wrap, long channel_number)
{
    int ret;

    SEM_WAIT(&wrap->core_lock);
    ret = blu_channel_status(wrap->session, channel_number);
    SEM_POST(&wrap->core_lock);
    return ret;
}

void waitfor_chan_stat_answered(WRAPPER *wrap, long channel_number)
{
    int chanstat;

    chanstat = blw_channel_status(wrap, channel_number);
    while (BLU_CHAN_STATUS_ANSWERED !=  chanstat && 
           BLU_CHAN_STATUS_QUIESCENT !=  chanstat &&
           0 != chanstat)
    {
        /*	 printf("Yielding -- wait for channel %ld status %d, got %d\n", 
        channel_number,BLU_CHAN_STATUS_ANSWERED, chanstat);
        */
        YIELD();
        chanstat = blw_channel_status(wrap, channel_number);
        if (wrap->conn.status == INST_STATUS_EXIT)
            chanstat=BLU_CHAN_STATUS_QUIESCENT;
    }
    return ;
}

void waitfor_chan_stat_quiescent(WRAPPER *wrap, long channel_number)
{
    int chanstat;

    chanstat = blw_channel_status(wrap, channel_number);
    if (wrap->conn.status == INST_STATUS_EXIT)
        chanstat=BLU_CHAN_STATUS_QUIESCENT;
    while (BLU_CHAN_STATUS_QUIESCENT !=  chanstat &&
           0 != chanstat)
    {
        YIELD();
        chanstat = blw_channel_status(wrap, channel_number);
        if (wrap->conn.status == INST_STATUS_EXIT)
            chanstat=BLU_CHAN_STATUS_QUIESCENT;
    }
    return ;
}

/*
 * blw_lmem_init
 * blw_malloc
 * blw_free
 * blw_lmem_cleanup
 *
 * Memory management routines provided as a convenience for 
 * the profile implementor.
 *
 * 
 * blw_lmem_init
 * blw_lmem_cleanup
 *
 * Create the bookkeeping lists and free/cleanup.
 *
 * blw_lmem_init returns NULL on success and an error code otherwise.
 */
int blw_lmem_init(WRAPPER *wrap) {
    return(int)(wrap->lmem = NULL);
}

void blw_lmem_cleanup(WRAPPER *wrap) {
    bp_slist * tmp, * list;

    SEM_WAIT(&sem_blw_malloc);

    list = wrap->lmem;
    while (list)
    {
        tmp = list;
        list = list->next;
        if (NULL != tmp->data)
            lib_free(tmp->data);
    }
    bp_slist__free(wrap->lmem);
    wrap->lmem = NULL;

    SEM_POST(&sem_blw_malloc);
}

/*
 * blw_malloc
 * blw_free
 *
 * These dow what you think, and the bookkeeping.
 */
void * blw_malloc(WRAPPER *wrap, size_t size) {
    void * tmp;
    
    SEM_WAIT(&sem_blw_malloc);

    if (NULL != (tmp = lib_malloc(size)))
        wrap->lmem = bp_slist__prepend (wrap->lmem, tmp);

    SEM_POST(&sem_blw_malloc);
    return(tmp);
}

void blw_free(WRAPPER *wrap, void *ptr) {
    bp_slist * tmp;

    SEM_WAIT(&sem_blw_malloc);

    if (NULL != (tmp = bp_slist__find (wrap->lmem, ptr))) {
        lib_free(ptr);
        wrap->lmem = bp_slist__remove_link(wrap->lmem, tmp);
    }

    SEM_POST(&sem_blw_malloc);
}

DIAGNOSTIC *bp_diagnostic_new(BP_CONNECTION *conn,
                              int code,
                              char *lang,
                              char *fmt, ...)
{
    char        buffer[BUFSIZ];
    va_list     ap;

    va_start (ap, fmt);
    vsnprintf (buffer, sizeof buffer, fmt, ap);
    va_end (ap);

    return blw_diagnostic_new(conn == NULL ? NULL : GET_WRAPPER (conn),
                              code, lang, "%s", buffer);
}


void bp_diagnostic_destroy(BP_CONNECTION* conn, DIAGNOSTIC *diagnostic)
{
    blw_diagnostic_destroy(conn == NULL ? NULL : GET_WRAPPER (conn),
                           diagnostic);
}

/*
 * Constructors and destructors.
 */
DIAGNOSTIC * blw_diagnostic_new(WRAPPER *wrap, int code, char *lang,
                                char *fmt, ...) 
{
    int len;
    char buffer[BUFSIZ],
        *message,
        *tmp;
    va_list     ap;
    DIAGNOSTIC *diag;

    va_start (ap, fmt);
    vsnprintf (buffer, sizeof buffer, fmt, ap);
    va_end (ap);

    len = sizeof(DIAGNOSTIC);
    if (lang && !*lang)
        lang = NULL;
    if (NULL != lang)
        len += strlen(lang) +1;
    message = buffer[0] != '\0' ? buffer : NULL;
    if (NULL != message)
        len += strlen(message) +1;

    if (NULL != (diag = lib_malloc(len))) {
        memset(diag,0,len);
        tmp = (char *)((char *)diag + sizeof(DIAGNOSTIC));
        diag->code = code;
        if (NULL == lang) {
            diag->lang = NULL;
        } else {
            diag->lang = tmp;
            strcpy(tmp, lang);
            tmp += strlen(lang) + 1;
        }
        if (NULL == message) {
            diag->message = NULL;
        } else {
            diag->message = tmp;
            strcpy(tmp, message);
            tmp += strlen(message) + 1;
        }
    }
    
    return(diag);
}

void blw_diagnostic_destroy(WRAPPER * wrap, DIAGNOSTIC *diagnostic)
{
    if (NULL != diagnostic)
        lib_free(diagnostic);
}

/*
 * chan0_msg constructor/destructor
 */
CHAN0_MSG * blw_chan0_msg_clone(WRAPPER *wrap,  CHAN0_MSG * msg)
{
    return blw_chan0_msg_new(wrap, msg->channel_number, msg->message_number,
                             msg->op_ic, msg->op_sc, msg->profile, msg->error, msg->features,
                             msg->localize, msg->server_name);
}

CHAN0_MSG * blw_chan0_msg_new(WRAPPER *wrap,  long channel_number, long message_number,
                              char op_ic, char op_sc, struct profile * profile,   
                              DIAGNOSTIC * error, char * features,
                              char * localize, char * server_name)
{
    int len, structarea = 0;
    char * tmp;
    struct chan0_msg * cz;
    struct profile * myprof, * tmpprof;

    len = sizeof(struct chan0_msg);
    if (localize) 
        len +=  strlen(localize) + 1;
    if (features) 
        len +=  strlen(features) + 1;
    if (server_name)
        len += strlen(server_name) + 1;
  
    if (error) {
        structarea = sizeof(DIAGNOSTIC);
        len += structarea;

        if (error->message)
            len += strlen(error->message) + 1;
        if (error->lang)
            len += strlen(error->lang) + 1;
    } else  {
        for (myprof = profile; myprof; myprof = myprof->next) {
            structarea += sizeof(struct profile);
            len += strlen(myprof->uri) + myprof->piggyback_length + 2;
        }
        len += structarea;
    }  
  
    if (NULL != (cz = lib_malloc(len))) {
        memset(cz, 0, len);
        tmp = (char*)((char *)cz + sizeof(struct chan0_msg) + structarea);

        SEM_WAIT(&wrap->lock);
        cz->session = wrap->session;
        SEM_POST(&wrap->lock);

        cz->channel_number = channel_number;
        cz->message_number = message_number;
        cz->op_ic = op_ic;
        cz->op_sc = op_sc;

        if (localize) {
            cz->localize = strcpy(tmp, localize);
            tmp +=  strlen(localize) + 1;
        }
        if (features) {
            cz->features = strcpy(tmp, features);
            tmp +=  strlen(features) + 1;
        }
        if (server_name) {
            cz->server_name = strcpy(tmp, server_name);
            tmp +=  strlen(server_name) + 1;
        }

        if (error) {
            cz->error = (DIAGNOSTIC *)((char *)cz + sizeof(CHAN0_MSG));

            cz->error->code = error->code;
      
            if (error->message) {
                cz->error->message = strcpy(tmp, error->message);
                tmp +=  strlen(error->message) + 1;
            }
            if (error->lang) {
                cz->error->lang = strcpy(tmp, error->lang);
                tmp +=  strlen(error->lang) + 1;
            }
        } else if (profile) {
            tmpprof = (struct profile *)((char *)cz + sizeof(CHAN0_MSG));
            cz->profile = tmpprof;
            for (myprof = profile; myprof; myprof = myprof->next) {
                tmpprof->encoding = myprof->encoding;
                tmpprof->piggyback_length = myprof->piggyback_length;

                tmpprof->uri = strcpy(tmp, myprof->uri);
                tmp +=  strlen(myprof->uri) + 1;

                if (tmpprof->piggyback_length) {
                    tmpprof->piggyback = tmp;
                    memcpy(tmp, myprof->piggyback, myprof->piggyback_length);
                    tmp += myprof->piggyback_length;
                }

                if (myprof->next) {
                    tmpprof->next = tmpprof + sizeof(struct profile);
                    tmpprof = tmpprof->next;
                } else {
                    tmpprof->next = NULL;
                }
            }
        }  
    }
    return(cz);
}

void blw_chan0_msg_destroy(WRAPPER *wrap, struct chan0_msg *msg)
{
    if (NULL != msg)
        lib_free(msg);
}

/*
 * profile_registration constructor/destructor
 */
PROFILE_REGISTRATION* bp_profile_registration_new(BP_CONNECTION* conn,
                                                  char* uri,
                                                  char* imodes, char* lmodes,
                                                  int fullmsg, int tpc)
{
    WRAPPER* wrap = GET_WRAPPER(conn);
    int len;
    char * tmp;
    PROFILE_REGISTRATION *myreg;

    len = sizeof(PROFILE_REGISTRATION);
    if (uri) 
        len +=  strlen(uri) + 1;
    if (imodes) 
        len +=  strlen(imodes) + 3;
    if (lmodes) 
        len +=  strlen(lmodes) + 3;
  
    if (wrap) {
        myreg = blw_malloc(wrap, len);
    } else {
        myreg = lib_malloc(len);
    }

    if (NULL != myreg) {
        memset(myreg, 0, len);

        myreg->allocator = &wrap->conn;
        myreg->full_messages = fullmsg;
        myreg->thread_per_channel = tpc;
	
        tmp = (char*)((char *)myreg + sizeof(PROFILE_REGISTRATION));

        if (uri) {
            myreg->uri = strcpy(tmp, uri);
            tmp +=  strlen(uri) + 1;
        }
        if (imodes) {
            myreg->initiator_modes = tmp;
            sprintf(tmp, ",%s,", imodes);
            tmp += strlen(imodes) + 3;
        }
        if (lmodes) {
            myreg->listener_modes = tmp;
            sprintf(tmp, ",%s,", lmodes);
            tmp += strlen(lmodes) + 3;
        }
    }

    return(myreg);
}

PROFILE_REGISTRATION* bp_profile_registration_clone(BP_CONNECTION* conn,
                                                    PROFILE_REGISTRATION *reg)
{
    WRAPPER* wrap = GET_WRAPPER(conn);
    int len;
    char * tmp;
    PROFILE_REGISTRATION *myreg = NULL;

    if (reg) {
        len = sizeof(PROFILE_REGISTRATION);
        if (reg->uri) 
            len +=  strlen(reg->uri) + 1;
        if (reg->initiator_modes) 
            len +=  strlen(reg->initiator_modes) + 3;
        if (reg->listener_modes) 
            len +=  strlen(reg->listener_modes) + 3;

        if (wrap) {
            myreg = blw_malloc(wrap, len);
        } else {
            myreg = lib_malloc(len);
        }

        if (NULL != myreg) {

            memcpy(myreg, reg, sizeof(PROFILE_REGISTRATION));

            myreg->allocator = &wrap->conn;
            myreg->full_messages = reg->full_messages;
            myreg->thread_per_channel = reg->thread_per_channel;

            tmp = (char*)((char *)myreg + sizeof(PROFILE_REGISTRATION));

            if (reg->uri) {
                myreg->uri = strcpy(tmp, reg->uri);
                tmp +=  strlen(reg->uri) + 1;
            }
            if (reg->initiator_modes) {
                myreg->initiator_modes = tmp;
                if (',' == reg->initiator_modes[0]) {
                    strcpy(tmp, reg->initiator_modes);
                    tmp +=  strlen(reg->initiator_modes) + 1;
                } else {
                    sprintf(tmp, ",%s,", reg->initiator_modes);
                    tmp += strlen(reg->initiator_modes) + 3;
                }
            }
            if (reg->listener_modes) {
                myreg->listener_modes = tmp;
                if (',' == reg->listener_modes[0]) {
                    strcpy(tmp, reg->listener_modes);
                    tmp +=  strlen(reg->listener_modes) + 1;
                } else {
                    sprintf(tmp, ",%s,", reg->listener_modes);
                    tmp += strlen(reg->listener_modes) + 3;
                }
            }
        }
    }

    return(myreg);
}

void bp_profile_registration_destroy(BP_CONNECTION* conn,
                                     PROFILE_REGISTRATION *reg)
{
    WRAPPER* wrap = GET_WRAPPER(conn);

    if (NULL != reg) {
        if (NULL != wrap) {
            blw_free(wrap, reg);
        } else {
            lib_free(reg);
        }
    }
}

void bp_profile_registration_chain_destroy(BP_CONNECTION* conn, 
                                           PROFILE_REGISTRATION* reg)
{  
    WRAPPER* wrap = GET_WRAPPER(conn);
    PROFILE_REGISTRATION *myreg, *tmpreg;

    if (reg) {
        if (wrap == GET_WRAPPER(reg->allocator)) {
            myreg = reg;
            while (NULL != myreg) {
                tmpreg = myreg->next;
                if (NULL != wrap) {
                    blw_free(wrap, myreg);
                } else {
                    lib_free(myreg);
                }
                myreg = tmpreg;
            }
        }
    }
}

/*
 * Local functions.
 */
PROFILE_REGISTRATION * map_uri_to_reg(WRAPPER *wrap, char *uri)
{
    bp_slist * list;

    list = wrap->profile_registration_active;
    while (list) {
        if (0 == strcmp(uri, ((PROFILE_REGISTRATION*)list->data)->uri))
            break;
        list = list->next;
    }
    if (list)
        return (PROFILE_REGISTRATION*)list->data;
    else
        return NULL;
}

int blw_thread_notify(WRAPPER *wrap, CHANNEL_INSTANCE *inst, 
                      NOTIFYCALL myfunc, void *ClientData)
{
    NOTIFY_REC  *nr;
    int err;

    nr = (NOTIFY_REC *)lib_malloc(sizeof(NOTIFY_REC));
    nr->channel = inst->channel_number;
    nr->channel_instance = inst;
    nr->s = wrap->session;
    nr->wrap = wrap;
    nr->cbfunc = myfunc;
    nr->client_data = ClientData;
    nr->client_data_free = lib_free;
    err = workqueue_add_item(callback_queue, nr_callback, nr);
    return err;
}

int blw_put_remembered_error(WRAPPER *wrap,long channel,DIAGNOSTIC *diag)
{

    bp_slist *list=NULL;
    char *err;

    list = bp_hash_int_get_value(wrap->error_remembered,channel);
    list = bp_slist__prepend(list,diag);
    err = bp_hash_int_entry(wrap->error_remembered,channel,list);
    if (err)
    {
        lib_free(err);
        wrap->error_remembered = bp_hash_resize(&wrap->error_remembered,1);
        err = bp_hash_int_entry(wrap->error_remembered,channel,list);
        if (err)
        {
            wrap->log(LOG_WRAP,1,"remembered error: %s",err);
            lib_free(err);
            return -1;
        }
    }
    return 0;
}

DIAGNOSTIC *blw_get_remembered_diagnostics(WRAPPER *wrap,long channel,int code,char *lang)
{
    DIAGNOSTIC *diag=NULL;
    char *msg;
    bp_slist *tmplist;
    int len = 3;
    bp_slist *list;

    list = blw_get_remembered_error(wrap,channel);
    tmplist=list;
    if (tmplist)
    {
        while(tmplist)
        {
            diag = (DIAGNOSTIC *)tmplist->data;
            if (diag->message)
                len += strlen(diag->message)+3;
            tmplist=tmplist->next;
        }
        msg = lib_malloc(len);
        tmplist=list;

        strcpy(msg," ");
        do
        {
            diag = (DIAGNOSTIC *)tmplist->data;
            strcat(msg,diag->message);
            strcat(msg,"\r\n");
            tmplist = tmplist->next;
        } while (tmplist);
        diag = blw_diagnostic_new(wrap,code,lang,"%s",msg);
        lib_free(msg);
        blw_remove_remembered_error(wrap,channel);

    }
    return diag;     
}

bp_slist *blw_get_remembered_error(WRAPPER *wrap, long channel)
{
    bp_slist *list=NULL;

    list = bp_hash_int_get_value(wrap->error_remembered,channel);
    return list;
}

int blw_remove_remembered_error(WRAPPER *wrap, long channel)
{
    bp_slist *tmplist,*list=NULL;
    DIAGNOSTIC *diag;

    list = bp_hash_int_get_value(wrap->error_remembered,channel);
    tmplist = list;
    while (list)
    {
        diag = (DIAGNOSTIC *)list->data;
        if (diag)
            blw_diagnostic_destroy(wrap,diag);
        list=list->next;
    }
    bp_slist__free(tmplist);
    bp_hash_int_entry_delete(wrap->error_remembered,channel);
        
    return 0;
}

void
session_shutdown(WRAPPER* wrap)
{
    bp_shutdown(&wrap->conn);
    fiostate_delete(wrap->iostate);
    lib_free((char *)wrap->conn.mode);
    wrap->conn.mode = NULL;
    if (wrap->conn.role == INST_ROLE_LISTENER) {
        bp_connection_destroy(&wrap->conn);
    }
}

void
session_start(WRAPPER* wrap)
{
    struct session *thissession;
    char **myprofiles=NULL;

    /* get profile list for new connection */
    /* for now this is blw_start -- will be moved later */
    myprofiles = blw_pro_session_init(wrap);
    SEM_WAIT(&wrap->lock);
    if (wrap->conn.status != INST_STATUS_RUNNING &&
        wrap->conn.status != INST_STATUS_EXIT)
    {
        wrap->conn.status = INST_STATUS_STARTING;
    }
    SEM_POST(&wrap->lock);
    SEM_WAIT(&wrap->core_lock);
    thissession = bll_session_create(malloc, free, notify_lower,
                                     notify_upper, wrap->conn.role, NULL,
                                     NULL, myprofiles,
                                     NULL, wrap);
    SEM_POST(&wrap->core_lock);
    if (myprofiles) {
        lib_free(myprofiles);
        myprofiles = NULL;
    }
    if (thissession == NULL)
    {
        bp_connection_destroy(&wrap->conn);
        exit(1);
    }
    /* find polling thread where a new socket can be added*/
    SEM_WAIT(&wrap->lock);
    wrap->session=thissession;
    SEM_POST(&wrap->lock);
    fiostate_set_wrapper(wrap->iostate, wrap);

    fiostate_start(wrap->iostate);
    if (wrap->conn.role == 'I')
        bp_wait_for_greeting(&wrap->conn);
    /* force seq after greeting to go out */
    fiostate_unblock_write(wrap->iostate);
    if (wrap->conn.status == INST_STATUS_EXIT) {
        session_shutdown(wrap);
    }
}

void session_tuning_reset_wkr(void *data)
{
    WRAPPER *wrap = (WRAPPER *)data;
    bp_slist * tmplist;
    PROFILE_REGISTRATION *tmpreg;
    IO_STATE *ios;
    int socket = wrap->iostate->socket;

    /* 
     * Need to do a limited cleanup and re-init of the wrapper here.
     */

    /* notify the registered profiles. */
    tmplist = wrap->profile_registration_active;
    while (tmplist) {
        tmpreg = (PROFILE_REGISTRATION *)tmplist->data;
        tmpreg->proreg_session_fin(tmpreg, &wrap->conn);
        tmplist=tmplist->next;
    }

    /* destroy the core session */
    SEM_WAIT(&wrap->core_lock);
    bll_session_destroy(wrap->session);
    SEM_POST(&wrap->core_lock);

    /* reregister profiles for the new session */
    ios = fiostate_new(socket);
    SEM_WAIT(&wrap->lock);
    wrap->iostate = ios;

    /*
     * This is a temporary fix.
     * A better solution is to not destroy the IO_STATE on tuning resets.
     */
    ios->stack_reader_writer = wrap->reader_writer;
    wrap->reader_writer = NULL;

    SEM_POST(&wrap->lock);
    session_start(wrap);
}

/*
 * Called by the transport layer to notify the wrapper that the underlying
 * connection is no longer being processed for I/O.
 */
void shutdown_callback(WRAPPER* wrap)
{
    if (wrap->conn.status == INST_STATUS_TUNING) {
        int err;
        err =
            workqueue_add_item(callback_queue, session_tuning_reset_wkr, wrap);
        ASSERT(err == 0);
    } else if (wrap->conn.status == INST_STATUS_EXIT) {
        fiostate_delete(wrap->iostate);
        wrap->iostate = NULL;
    } else {
        wrap->conn.status = INST_STATUS_EXIT;
#if 0
        /** @todo fix this... */
        session_shutdown(wrap);
#endif
    }
}

int bp_shutdown(BP_CONNECTION* conn)
{
    WRAPPER* wrap = GET_WRAPPER(conn);
    DIAGNOSTIC *diag=NULL/*,mydiag*/;
    PROFILE_INSTANCE *mypro;
    CHANNEL_INSTANCE * inst;
    bp_slist *list;
    int count=0;

    if (wrap->conn.status == INST_STATUS_EXIT)
        return BLW_OK; 
    if (wrap->channel_ref_count <= 1)
        return BLW_OK;

    diag = (DIAGNOSTIC *)lib_malloc(sizeof(DIAGNOSTIC));
    memset(diag,0,sizeof(DIAGNOSTIC *));
    SEM_WAIT(&wrap->lock);
    list = wrap->conn.channel_instance;
    while (list)
    {
        inst = (CHANNEL_INSTANCE *)list->data;
        if (inst->channel_number)
        {
            SEM_TRYWAIT(&inst->user_sem2);
            SEM_POST(&inst->user_sem2);
            SEM_TRYWAIT(&inst->user_sem1);
            SEM_POST(&inst->user_sem1);
            SEM_POST(&inst->user_sem1);
            mypro = inst->profile;
            if (mypro)
                if (mypro->pro_close_confirmation) {
                    SEM_WAIT(&wrap->callback_lock);
                    mypro->pro_close_confirmation(inst->profile,
                                                  PRO_ACTION_SUCCESS, NULL,
                                                  PRO_ACTION_LOCAL,
                                                  PRO_ACTION_SESSION);
                    SEM_POST(&wrap->callback_lock);
                }
            SEM_WAIT(&wrap->core_lock);
            blu_channel_close(wrap->session, inst->channel_number, diag);
            SEM_POST(&wrap->core_lock);
        }
        list = list->next;
    }
    SEM_POST(&wrap->lock);
    lib_free(diag);
    diag=NULL;

#ifndef WIN32
/*    if (wrap->pn->max != 1)
    {
        mydiag.code = 200;
        mydiag.lang = NULL;
        mydiag.message = NULL;
        cz = blw_chan0_msg_new(wrap, 0, -1, 'i' , 'c',  NULL,
                                 &mydiag, "", "", NULL);
        diag = blw_close_request(wrap, cz, NULL, NULL);
    } */
#endif

    while (wrap->channel_ref_count > 1) {
        if (diag) {
           DIAGNOSTIC srcdiag;
           struct chan0_msg *request;

           blw_diagnostic_destroy(wrap, diag);

           srcdiag.code = 200;
           srcdiag.lang = NULL;
           srcdiag.message = NULL;

           request = blw_chan0_msg_new(wrap, 0, -1, 'i' , 'c',  NULL,
                                       &srcdiag, "", "", NULL);

           diag = blw_close_request_prim(wrap, request, NULL, NULL);

        } else {
            YIELD();
            count++;
            if (count > 100)
            {
                list = wrap->conn.channel_instance;
                count = bp_slist__length(list);
                if (count < wrap->channel_ref_count) {
                    wrap->channel_ref_count = count;
                }

                while (list) {
                     inst = (CHANNEL_INSTANCE *)list->data; 
                     if (inst->channel_number)
                     {
                         SEM_WAIT(&wrap->lock);
                         wrap->conn.channel_instance =
                             bp_slist__remove(wrap->conn.channel_instance, inst);
                         channel_instance_destroy(inst);
                         SEM_POST(&wrap->lock);
/* what in the world is this code ? */
                         count = wrap->channel_ref_count;
                         channel_instance_destroy(inst);
                         if (count == wrap->channel_ref_count);
                             wrap->channel_ref_count--;
                         list = NULL;
                     } else {
                         list=list->next;
                     }
                 }
             }
        }
    }

    wrap->conn.status=INST_STATUS_EXIT;
    session_shutdown(wrap);

    return BLW_OK;
}

void bp_start(BP_CONNECTION *conn, char role)
{
    WRAPPER *wrap = GET_WRAPPER(conn);

    wrap->conn.status = INST_STATUS_STARTING;
    wrap->conn.role = (role == 'I') ? INST_ROLE_INITIATOR : INST_ROLE_LISTENER;
    session_start(wrap);
}

void bp_log(BP_CONNECTION *conn,
            int service,
            int severity,
            char *fmt, ...) {
    char    buffer[BUFSIZ];
    va_list ap;
    WRAPPER *wrap = GET_WRAPPER(conn);

    va_start (ap, fmt);
    vsnprintf (buffer, sizeof buffer, fmt, ap);
    va_end (ap);

    wrap->log (service, severity, "%s", buffer);
}

void bp_wait_channel_0_state_quiescent(BP_CONNECTION* conn)
{
    waitfor_chan_stat_quiescent(GET_WRAPPER(conn), 0);
}

IO_STATE* bp_get_iostate(BP_CONNECTION* conn)
{
    return GET_WRAPPER(conn)->iostate;
}
