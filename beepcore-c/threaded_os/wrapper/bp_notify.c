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
 * $Id: bp_notify.c,v 1.10 2002/04/20 21:46:55 huston Exp $
 *
 * IW_notify.c
 *
 */
#ifndef WIN32
#include <unistd.h>
#endif
#include <stdio.h>
#include "../../utility/bp_queue.h"
#include "../../core/generic/CBEEP.h"
#include "../../core/generic/CBEEPint.h"
#include "../transport/bp_fiostate.h"
#include "../transport/bp_fcbeeptcp.h"
#include "../utility/semutex.h"
#include "../utility/workthread.h"
#include "bp_notify.h"

#define STATIC

extern WORKQUEUE* callback_queue;
extern sem_t upper_sem;

int IW_notify_handle_chan0(void *nrvoid);
int IW_notify_handle_frame(void *nrvoid);
int IW_notify_handle_message(void *nrvoid);
int IW_notify_chan0_request(void *nrvoid);
int IW_notify_chan0_response(void *nrvoid);

STATIC void nr_callback(void* data)
{
    NOTIFY_REC* nr = (NOTIFY_REC*) data;

    nr->cbfunc(nr);
    nr->client_data_free(nr);
}

void process_notify_upper(struct session* s, long c, int i)
{
    WRAPPER* wrap = (WRAPPER *)blu_session_info_get(s);
    IO_STATE *thisio=wrap->iostate;
    CHANNEL_INSTANCE *thisinstance=NULL;
    NOTIFY_REC *nr=NULL;
    int err,oktoq,status,fc;

    if (c != 0)
        thisinstance = (CHANNEL_INSTANCE *)blu_channel_info_get(s,c);

    nr = (NOTIFY_REC *)s->malloc(sizeof(NOTIFY_REC));
    nr->channel = c;
    nr->op = i;
    nr->channel_instance = thisinstance;
    nr->s = s;
    nr->wrap = wrap;

    if (thisinstance)
        nr->client_data = &thisinstance->commit;
    else
        nr->client_data = NULL;

    oktoq = 0;
    switch (i) {
        case BLU_NOTEUP_MESSAGE: 
            if (c == 0)
            {
                nr->cbfunc = IW_notify_handle_chan0;
                oktoq=1;
            }
            else
            {
                if (thisinstance)
                {
                    if (thisinstance->profile->full_messages)
                    {
                        nr->cbfunc = IW_notify_handle_message;
                        oktoq=1;
                    }
                }
            }
            break;
        case BLU_NOTEUP_FRAME:
            if (thisinstance && c)
            {
                if (thisinstance->profile->pro_frame_available &&
                    thisinstance->profile->full_messages == 0)
                {
                    nr->cbfunc = IW_notify_handle_frame;
                    oktoq=1;
                }
            }
            break;
        case BLU_NOTEUP_QEMPTY:
            break;
        case BLU_NOTEUP_ANSWERED:
            if (thisinstance)
            {
                status = blu_channel_status(s,thisinstance->channel_number);
                        
                /* check if this profile is waiting for a chan0_request */
                if (0)
                    nr->cbfunc = IW_notify_chan0_request;
                oktoq=0;
            }
            break;
        case BLU_NOTEUP_QUIESCENT:
            if (thisinstance)
            {
                /* check if this profile is waiting for a chan0_reply */
                if (0)
                    nr->cbfunc = IW_notify_chan0_response;
                oktoq=0;
            }
            break;
        case BLU_NOTEUP_WINDOWFULL:
            if (thisinstance)
            {
                if (thisinstance->profile->full_messages)
                {
                    fc = blu_channel_frame_count(s,c,BLU_QUERY_ANYTYPE,
                                                 BLU_QUERY_COMPLETE);
                    if (fc == 0)
                        fiostate_stop(thisio);
                }
            }
            else
            {
                fc = blu_channel_frame_count(s,0,BLU_QUERY_ANYTYPE,
                                             BLU_QUERY_COMPLETE);
                if (fc == 0)
                    fiostate_stop(thisio);
            }
            fiostate_unblock_write(thisio);
            break;
        default:
#ifdef DEBUGFORK
            printf("unknown operation code in notify upper op->%d channel->%ld\n",i,c);
#endif
            break;
    }

    if (oktoq) {
        nr->client_data_free = s->free;
        err = workqueue_add_item(callback_queue, nr_callback, nr);
        ASSERT(err == 0);
    } else {
        s->free(nr);
    }
}

void notify_upper(struct session * s, long c, int i) 
{
    WRAPPER *wrap=NULL;
    CHANNEL_INSTANCE *thisinstance=NULL;
    PENDING_EVENT* event;

    ASSERT(callback_queue != NULL);

    SEM_WAIT(&upper_sem);
    wrap = (WRAPPER *)blu_session_info_get(s);
    if (wrap) {
        ASSERT(wrap->iostate != NULL);
        if (c != 0)
            thisinstance = (CHANNEL_INSTANCE *)blu_channel_info_get(s,c);

        if (thisinstance &&
            (thisinstance->status == INST_STATUS_STARTING
             || bp_queue_peek(thisinstance->pending_event) != NULL))
        {
            event = malloc(sizeof(PENDING_EVENT));
            if (event == NULL) {
                /* #$@%@ should terminate the session here */
                return;
            }

            event->s = s;
            event->c = c;
            event->i = i;
            bp_queue_put(thisinstance->pending_event, event);

            SEM_POST(&upper_sem);
            return;
        }
        process_notify_upper(s, c, i);
    }
#ifdef DEBUGFORK
    printf("Notify upper: %ld, %d ", c, i);
    if (thisinstance)
        printf("status %d ",blu_channel_status(s,thisinstance->channel_number));
    printf("\n");
#endif
    SEM_POST(&upper_sem);
    return;
}

/*
notify_upper {
  look up profile instance associated with notification
  if channel == 0 && complete message (op=2),
    enqueue handle_chan0 && signal upper semaphore
  if complete message && instance has handle_message slot
    enqueue handle_message && signal upper semaphore
  if partial message && instance handles partial messages
    enqueue handle_frame && signal upper semaphore
  if op=4 (half quiescent) and instance is waiting for that,
    enqueue dispatch_chan0_request && signal upper semaphore
  if op=5 (full quiescent) and instance is waiting for that,
    enqueue dispatch_chan0_response && signal upper semaphore
}
 */

int blw_wkr_post_close_confirm(void *nrvoid)
{
    NOTIFY_REC *nr = (NOTIFY_REC *)nrvoid;
    WRAPPER *wrap = nr->wrap;
    CHANNEL_INSTANCE * inst = nr->channel_instance;
    char commitflag = *(char *)nr->client_data;
    int semflag = 1;
    int status;
  
    SEM_WAIT(&wrap->lock);
    status = blw_channel_status(wrap, inst->channel_number);
    SEM_POST(&wrap->lock);
    if (inst->channel_number && 
        PRO_ACTION_SUCCESS == commitflag &&
        BLU_CHAN_STATUS_QUIESCENT != status &&
        BLU_CHAN_STATUS_CLOSED != status &&
        wrap->conn.status != INST_STATUS_EXIT)
    {
        /*	waitfor_chan_stat_quiescent(wrap, inst->channel_number); */
        blw_thread_notify(wrap, inst, blw_wkr_post_close_confirm, 
                          nr->client_data);
        return 0;
    }

    // how do I get a close data from here?
    SEM_WAIT(&wrap->callback_lock);
    inst->profile->pro_close_confirmation(inst->profile, commitflag, NULL,
                                          PRO_ACTION_LOCAL,
                                          PRO_ACTION_CHANNEL);
    SEM_POST(&wrap->callback_lock);

    if (commitflag == PRO_ACTION_SUCCESS) {
        SEM_POST(&inst->user_sem2);
        SEM_WAIT(&wrap->lock);
        wrap->conn.channel_instance =
            bp_slist__remove(wrap->conn.channel_instance, inst);
        channel_instance_destroy(inst);
        SEM_POST(&wrap->lock);
        semflag=0;
    }
    if (semflag)
        SEM_POST(&inst->user_sem2);

    return 0;
}

void blw_close_confirmation(WRAPPER *wrap, struct chan0_msg *confirm)
{
    bp_slist * list;
    CHANNEL_INSTANCE * inst;
    PROFILE_INSTANCE * mypro;
    char commit;
    struct chan_close close;

    /* $@$@$@$ Error checking here? The library should insure the
     * reply sequence
     */
    close.session = confirm->session;
    close.channel_number = confirm->channel_number;
    close.diag = confirm->error;
    close.localize = confirm->localize;

    SEM_WAIT(&wrap->lock);
    inst = bp_queue_peek(wrap->pending_chan0_actions); 
    SEM_POST(&wrap->lock);
    ASSERT(inst != NULL);
    if (confirm->error) {
        /* was in blw_action_error - rollback */
        inst->status = INST_STATUS_RUNNING;

        SEM_WAIT(&wrap->callback_lock);
        if (inst->app_cr_callback)
            inst->app_cr_callback(inst->cb_client_data, inst, confirm->error);
        SEM_POST(&wrap->callback_lock);

        if (inst->outbound)
            blw_chan0_msg_destroy(wrap, inst->outbound);

        blw_chan0_msg_destroy(wrap, confirm);

        SEM_POST(&inst->user_sem1);
        return;
    }

    if (confirm->error) {
        commit = PRO_ACTION_FAILURE;
    } 
    else {
        commit = PRO_ACTION_SUCCESS;
    }
    
    if (inst->outbound && inst->outbound->channel_number) {
        /* Cleanup */
        if (PRO_ACTION_SUCCESS == commit) {
            if (wrap->conn.status != INST_STATUS_EXIT &&
                inst->profile_registration->thread_per_channel)
            {
                inst->commit = commit;
                blw_thread_notify(wrap, inst, blw_wkr_post_close_confirm,
                                  &inst->commit);
                SEM_WAIT(&inst->user_sem2);
            } else {
                if (PRO_ACTION_SUCCESS == commit) {
                    if (wrap->conn.status != INST_STATUS_EXIT)
                        waitfor_chan_stat_quiescent(wrap, inst->outbound->channel_number);
                }

                mypro = inst->profile;
                SEM_WAIT(&wrap->callback_lock);
                mypro->pro_close_confirmation(inst->profile, commit,
                                              confirm->error,
                                              PRO_ACTION_LOCAL,
                                              PRO_ACTION_CHANNEL);
                SEM_POST(&wrap->callback_lock);
                if (commit == PRO_ACTION_SUCCESS) {
                    SEM_WAIT(&wrap->lock);
                    wrap->conn.channel_instance =
                        bp_slist__remove(wrap->conn.channel_instance, inst);
                    SEM_POST(&wrap->lock);
                }
            }
        }
    } else {
        SEM_WAIT(&wrap->lock);
        wrap->conn.channel_instance =
            bp_slist__remove(wrap->conn.channel_instance, inst);
        SEM_POST(&wrap->lock);
        /* wrap->lock should be held while walking the list */
        list = wrap->conn.channel_instance;
        while (list) {
            CHANNEL_INSTANCE* inst2 = (CHANNEL_INSTANCE *)list->data;	    
            if (inst2 && inst2->channel_number) {
                if (wrap->conn.status != INST_STATUS_EXIT &&
                    inst2->profile_registration->thread_per_channel)
                {
                    inst2->commit = commit;
                    blw_thread_notify(wrap, inst2, blw_wkr_post_close_confirm,
                                      &inst2->commit);
                    SEM_WAIT(&inst2->user_sem2);
                } else {
                    if (wrap->conn.status != INST_STATUS_EXIT)
                        waitfor_chan_stat_quiescent(wrap,
                        inst2->channel_number);
                    mypro = inst2->profile;
                    SEM_WAIT(&wrap->callback_lock);
                    mypro->pro_close_confirmation(inst2->profile, commit,
                                                  confirm->error,
                                                  PRO_ACTION_LOCAL,
                                                  PRO_ACTION_CHANNEL);
                    SEM_POST(&wrap->callback_lock);
                    if (commit == PRO_ACTION_SUCCESS) {
                        SEM_WAIT(&wrap->lock);
                        wrap->conn.channel_instance =
                            bp_slist__remove(wrap->conn.channel_instance, inst2);
                        channel_instance_destroy(inst2);
                        SEM_POST(&wrap->lock);
                    }
                }

                list = wrap->conn.channel_instance;
            } else {
                list = list->next;
            }
        }
    }

    SEM_WAIT(&wrap->lock);
    inst = bp_queue_get(wrap->pending_chan0_actions); 
    SEM_POST(&wrap->lock);

    SEM_WAIT(&wrap->callback_lock);
    if (inst->app_cr_callback)
        inst->app_cr_callback(inst->cb_client_data, inst, NULL);
    SEM_POST(&wrap->callback_lock);

    if (PRO_ACTION_SUCCESS == commit) {
        SEM_WAIT(&wrap->lock);
        channel_instance_destroy(inst);
        SEM_POST(&wrap->lock);
    }

    /*    blw_chan0_msg_destroy(wrap, inst->outbound); */
    blw_chan0_msg_destroy(wrap, confirm);
}

/**
 * Wrapper function called to handle a close request message on channel 0.
 * The </i>pro_close_indication<i> function is called for the channel. 
 * If the diagnostic returned therefrom is NULL, then </i>pro_close_confirmation<i>
 * is called for the channel, an &lt;ok/&gt; message is sent in reply, and the
 * channel instance is reaped.  If the diagnostic returned is not NULL, then 
 * the diagnostic returned as an &lt;error&gt; message is sent in the RPY. 
 *
 * @param wrap A pointer to the wrapper structure.
 *
 * @param chan0_msg A pointer to the request structure. The pointer is passed 
 *                  to both the callback and the <I>pro_close_confirmation</I>
 *                  routine.  The channel 0 message structs will be deallocated
 *                  after these routines are called.
 * 
 * @param request A pointer to the request.
 *
 * @see pro_close_confirmation
 **/
void blw_close_indication(WRAPPER *wrap, struct chan0_msg *indication)
{
    bp_slist * list, * rblist = NULL;
    CHANNEL_INSTANCE * inst;
    DIAGNOSTIC * mydiag = NULL;
    char sesschan, commitflag;
    PROFILE_INSTANCE *mypro;

    SEM_WAIT(&wrap->lock);
    /* construct our action list */
    if (indication->channel_number) {
        /* closing the channel */
        sesschan = PRO_ACTION_CHANNEL;
        if (NULL == (inst = blw_map_channel_number(wrap, indication->channel_number))) {
            mydiag = blw_diagnostic_new(wrap, 550, NULL,
                                        "channel not open: %d",
                                        indication -> channel_number);
        } else {
            mydiag = blw_wkr_cr_instance(wrap, inst, indication, NULL, 
                                         PRO_ACTION_REMOTE);
        }
        SEM_POST(&wrap->lock);
        rblist = bp_slist__prepend(rblist, inst);
    }
    else {
        /* closing down the session */
        sesschan = PRO_ACTION_SESSION;

        /* tell em all */
        list = wrap->conn.channel_instance;
        while (list) {
            inst = (CHANNEL_INSTANCE *)list->data;
            if (inst && inst->channel_number) {
                SEM_POST(&wrap->lock);
                mydiag = blw_wkr_cr_instance(wrap, inst, indication, NULL, 
                                             PRO_ACTION_REMOTE);
                SEM_WAIT(&wrap->lock);
                if (mydiag) {
                    break;
                }
                rblist = bp_slist__prepend(rblist, inst);
            }
            list = list->next;
        }
    }
    
    SEM_POST(&wrap->lock);

    if (mydiag) {
        commitflag = PRO_ACTION_FAILURE;
    } else {
        commitflag = PRO_ACTION_SUCCESS;
    }

    /* 
     * now need to call confirm either on the channel or all channels
     * we have touched for a close on 0.
     */
    list = rblist;
    while (list) {
        inst = (CHANNEL_INSTANCE *)list->data;
        if (inst && inst->channel_number) {
            if (wrap->conn.status != INST_STATUS_EXIT &&
                inst->profile_registration->thread_per_channel)
            {
                inst->commit = commitflag;
                blw_thread_notify(wrap, inst, blw_wkr_post_close_confirm, 
                                  &inst->commit);
                /*  SEM_POST(&wrap->wrap_lock); */
                SEM_WAIT(&inst->user_sem2);
                /*  SEM_WAIT(&wrap->wrap_lock); */
            } else {
                if (PRO_ACTION_SUCCESS == commitflag)
                    waitfor_chan_stat_quiescent(wrap, inst->channel_number);

                mypro = inst->profile;
                mypro->pro_close_confirmation(inst->profile, commitflag,
                                              indication->error,
					      PRO_ACTION_REMOTE,
					      PRO_ACTION_CHANNEL);
                if (commitflag == PRO_ACTION_SUCCESS) {
                    SEM_WAIT(&wrap->lock);
                    wrap->conn.channel_instance =
                        bp_slist__remove(wrap->conn.channel_instance, inst);
                    channel_instance_destroy(inst);
                    SEM_POST(&wrap->lock);
                }
            }
        }
        list = list->next;
    }

    blw_chan0_reply(wrap, indication, NULL, mydiag);

    /* cleanup */
    if (rblist) 
        bp_slist__free(rblist);

    return;
}

int IW_notify_handle_chan0(void *nrvoid)
{
    NOTIFY_REC *nr;
    struct chan0_msg *cz;
  
    nr = (NOTIFY_REC *)nrvoid;
    cz = blu_chan0_in(nr->s);

    /* $@$@$@$ need to handle greeting reply case, HULL return */

    /* 
     * NB that blu_chan0_in should return a valid struct chan0_msg
     * So we do not check as rigorously for error cases here.
     */
    if (cz)
    {
        switch (cz->op_ic) 
        {
        case 'i':
            switch (cz->op_sc) 
            {
            case 's':
                blw_start_indication(nr->wrap,cz);
                break;
            case 'c':
                blw_close_indication(nr->wrap,cz);
                break;
            }
            break;
            case 'c':
                switch (cz->op_sc) 
                {
                case 's':
                    blw_start_confirmation(nr->wrap,cz);
                    break;
                case 'c':
                    blw_close_confirmation(nr->wrap,cz);
                    break;
                case 'g':
                    blw_greeting_notification(nr->wrap,cz);
                    break;
                }
                break;
#if 0
/*
                case 'e':
                blw_action_error(nr->wrap,cz);
                break;
*/
#endif
        }
    }
    else
    {
        blw_greeting_notification(nr->wrap,cz);
    }

    return 0;
}

int IW_notify_handle_frame(void *nrvoid)
{
    NOTIFY_REC *nr;

    CHANNEL_INSTANCE *cinstance;
    PROFILE_INSTANCE *pinstance;

    nr = (NOTIFY_REC *)nrvoid;

    if (nr->wrap->conn.status == INST_STATUS_EXIT)
        return 0;	    

    cinstance = nr->channel_instance;
    pinstance = nr->channel_instance->profile;

    if (cinstance->profile->pro_frame_available)
        cinstance->profile->pro_frame_available(pinstance);
    return 0;

}

int IW_notify_handle_message(void *nrvoid)
{
    NOTIFY_REC *nr;
    CHANNEL_INSTANCE *cinstance;
    PROFILE_INSTANCE *pinstance;

    nr = (NOTIFY_REC *)nrvoid;
    if (nr->wrap->conn.status == INST_STATUS_EXIT)
        return 0;

    cinstance = nr->channel_instance;
    pinstance = nr->channel_instance->profile;

    if (cinstance->profile->pro_message_available)
        cinstance->profile->pro_message_available(pinstance);
    return 0;
}

int IW_notify_chan0_request(void *nrvoid)
{
    NOTIFY_REC *nr;
    nr = (NOTIFY_REC *)nrvoid;

    return 0;
}


int IW_notify_chan0_response(void *nrvoid)
{
    NOTIFY_REC *nr;
    nr = (NOTIFY_REC *)nrvoid;

    return 0;
}


