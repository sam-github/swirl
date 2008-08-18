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
 * $Rev$
 *
 * bpc_wrapper.c
 *
 * Implementation of the wrapper around the CBEEP core stuff that allows
 * it to interact sanely with the world.
 *
 */

#include <assert.h>
#include "../../core/generic/CBEEPint.h"

#ifndef WIN32
#include <unistd.h>
#include <dlfcn.h>
#endif
#include "../utility/semutex.h"
#include <stdlib.h>
#include <string.h>
#include "bp_notify.h"
#include "bp_wrapper.h"
#include "../utility/logutil.h"
#include "../../utility/bp_hash.h"
#include "../../utility/bp_malloc.h"
#include "../../utility/bp_queue.h"
#include "../../utility/bp_slist_utility.h"
#include "../../core/generic/CBEEP.h"
#include "../transport/bp_fcbeeptcp.h" 
#include "../transport/bp_fiostate.h"


void blw_profiles_compute_active(WRAPPER *wrap);
void blw_wkr_init_profile(PROFILE_INSTANCE * pro, PROFILE_REGISTRATION * reg);
DIAGNOSTIC *blw_get_remembered_diagnostics(WRAPPER *wrap, long channel_number,
					   int code, char *lang);

void blw_chan0_reply(WRAPPER *wrap,struct chan0_msg *c0m,struct profile *pro,DIAGNOSTIC *diag);


void bpc_close_response(CHANNEL_INSTANCE * inst, DIAGNOSTIC * diag)
{
    WRAPPER * wrap;

    wrap = GET_WRAPPER(inst->conn);

    SEM_WAIT(&wrap->lock);
    if (diag) {
        inst->diagnostic = blw_diagnostic_new(wrap, diag->code, diag->lang, 
                                              "%s", diag->message);
    }
    else {
        inst->diagnostic = NULL;
    }
    SEM_POST(&wrap->lock);
    return;
}

void bpc_start_response(CHANNEL_INSTANCE * inst, struct profile* profile, 
                        DIAGNOSTIC * diag)
{
    WRAPPER * wrap;
    struct chan0_msg* confirmation;

    wrap = GET_WRAPPER(inst->conn);

    confirmation = blw_chan0_msg_new( wrap, inst->inbound->channel_number,
                                    inst->inbound->message_number, inst->inbound->op_ic,
                                    inst->inbound->op_sc, profile,
                                    diag, inst->inbound->features,
                                    inst->inbound->localize, inst->inbound->server_name );
    if (diag) {
        inst->diagnostic = blw_diagnostic_new(wrap, diag->code, diag->lang, 
                                              "%s", diag->message);
        inst->outbound = NULL;
    }
    else {
        inst->diagnostic = NULL;
        inst->outbound = confirmation;
    }

    return;
}

void bpc_tuning_reset_request(CHANNEL_INSTANCE *tunerinst)
{
    WRAPPER* wrap = GET_WRAPPER(tunerinst->conn);
    bp_slist * list;
    CHANNEL_INSTANCE * inst;

    /* SEM_WAIT(&wrap->wrap_lock); */

    wrap->conn.status = INST_STATUS_TUNING;

    /* tell em all */
    list = wrap->conn.channel_instance;
    while (list) {
        inst = (CHANNEL_INSTANCE *)list->data;
        if (inst) {
            if (tunerinst) {
                if (inst->channel_number == tunerinst->channel_number) {
                    list = list->next;
                    continue;
                }
            }
            if (inst->channel_number)
                inst->profile->pro_tuning_reset_indication(inst->profile);
        }
        list = list->next;
    }

    /* 
     * need to wait for all to drain before returning, since the core 
     * tuning reset thing is not there yet.
     */

    list = wrap->conn.channel_instance;
    while (list) {
        inst = (CHANNEL_INSTANCE *)list->data;
        if (inst) {
            /*
            if (tunerinst) {
                if (inst->channel_number == tunerinst->channel_number) {
                    blu_channel_statusflag_set(wrap->session, 
                    inst->channel_number,
                    INST_STATUS_TUNING);
                    list = list->next;
                    continue;
                }
            }
            */
            if (inst->channel_number && 
                (tunerinst->channel_number != inst->channel_number)) {
                waitfor_chan_stat_answered(wrap, inst->channel_number);
                inst->status = INST_STATUS_TUNING;
                /*
                blu_channel_statusflag_set(wrap->session, inst->channel_number,
                INST_STATUS_TUNING);
                */
            } 
        }
        list = list->next;
    }

    /*
    waitfor_chan_stat_quiescent(wrap, 0);
    blu_channel_statusflag_set(wrap->session, 0, INST_STATUS_TUNING);
    */

    blu_tuning_reset(wrap->session, tunerinst->channel_number);

    /* cleanup */
/*    SEM_POST(&wrap->wrap_lock); */

    return;
}

void bpc_tuning_reset_complete(CHANNEL_INSTANCE *tunerinst, char commit, 
                               char * mode)
{
    bp_slist * list;
    WRAPPER *wrap;
    CHANNEL_INSTANCE * inst;
    char newstat = INST_STATUS_TUNING;

    wrap = GET_WRAPPER(tunerinst->conn);

    if (PRO_ACTION_SUCCESS != commit) {
        commit = PRO_ACTION_FAILURE;
        newstat = INST_STATUS_RUNNING;
        wrap->conn.status = INST_STATUS_RUNNING;
    }

    /* tell em all */
    list = wrap->conn.channel_instance;
    while (list) {
        inst = (CHANNEL_INSTANCE *)list->data;
        if (inst) {
            if (inst->channel_number == tunerinst->channel_number) {
                list = list->next;
                continue;
            }

            inst->status = newstat;	    
            /*
            if (PRO_ACTION_SUCCESS != commit) 
            blu_channel_statusflag_set(wrap->session, inst->channel_number, 
            BLU_CHAN_STATUSFLAG_RUNNING);
            */

            if (inst->channel_number)
                inst->profile->pro_tuning_reset_confirmation(inst->profile,
                                                             commit);
        }
        list = list->next;
    }
	
    /* now clean up a tad if needed. */
    if (PRO_ACTION_SUCCESS == commit) {
        /* close down all of the channels */
        list = wrap->conn.channel_instance;
        while (list) {
            inst = (CHANNEL_INSTANCE *)list->data;

            if (inst) {
                if (inst->channel_number == tunerinst->channel_number) {
                    wrap->tuning_instance =
                        bp_slist__prepend(wrap->tuning_instance, inst);
                    wrap->conn.channel_instance =
                        bp_slist__remove(wrap->conn.channel_instance, inst);
                } else {
                    /*  @todo SEM_WAIT(&wrap->lock); */
                    wrap->conn.channel_instance =
                        bp_slist__remove(wrap->conn.channel_instance, inst);
                    channel_instance_destroy(inst);
                    /* @todo SEM_POST(&wrap->lock); */
                }
            }

            list = wrap->conn.channel_instance;
        }

        /* kill the queues etc... */
        bp_queue_free(wrap->pending_chan0_actions);
        wrap->pending_chan0_actions = bp_queue_new(BLW_CZ_ACTION_QSIZE);

        bp_hash_table_fin(&wrap->error_remembered);
        wrap->error_remembered = bp_hash_table_init(1,BP_HASH_INT);

        /* kill the session with any outstanding frames killed */

        /* rebuild the wrapper to get caught by blw_start */
        inst = channel_instance_new(wrap, NULL, NULL, NULL, NULL, NULL, NULL);
        inst->channel_number = 0;
        wrap->conn.channel_instance = bp_slist__prepend(NULL, inst);

        if (mode) {
            /* set up for restart */
            bp_mode_set(&wrap->conn, mode);
            blw_profiles_compute_active(wrap);
            wrap->conn.status = INST_STATUS_TUNING;
        } else {
            /* bailing out, set up for exit on null mode. */
            wrap->conn.status = INST_STATUS_EXIT;
        }
#if 0
        /** @todo this cannot be freed until after the callbacks are complete */
        inst = (CHANNEL_INSTANCE *)wrap->tuning_instance->data;
        channel_instance_destroy(inst);
#endif

        /*
         * This is a temporary fix.
         * A better solution is to not destroy the IO_STATE on tuning resets.
         */
	wrap->reader_writer = wrap->iostate->stack_reader_writer;
        fiostate_stop(wrap->iostate);
    } else {
        blu_tuning_reset(wrap->session, BLU_TUNING_RESET_ROLLBACK);
    }

    return;
}

struct frame *bpc_frame_read(CHANNEL_INSTANCE *inst)
{
    struct frame *f=NULL;
    WRAPPER * wrap;
    long channel_number;

    wrap = GET_WRAPPER(inst->conn);
    if (wrap->conn.status == INST_STATUS_EXIT)
        return f;
    channel_number = inst->channel_number;

    SEM_WAIT(&wrap->core_lock);
    f = blu_frame_read(wrap->session, channel_number);
    SEM_POST(&wrap->core_lock);
    return f;
}


struct frame *bpc_frame_allocate(CHANNEL_INSTANCE *inst, size_t size)
{
    struct frame *f=NULL;
    WRAPPER * wrap;

    if (!inst)
        return NULL;
    wrap = GET_WRAPPER(inst->conn);
    if (wrap->conn.status == INST_STATUS_EXIT)
        return f;

    SEM_WAIT(&wrap->core_lock);
    f = blu_frame_create(wrap->session, size);
    SEM_POST(&wrap->core_lock);
    return f;
}

char *bpc_buffer_allocate(CHANNEL_INSTANCE *channel_inst, size_t size)
{
    struct frame *f=bpc_frame_allocate(channel_inst, size);

    return f->payload;
}

char *bpc_error_allocate (CHANNEL_INSTANCE *channel_inst, int code,
                           char *language, char *diagnostic) {
    int          size;
    char        *buffer,
                *cp,
                *ep;

    /* estimate length for buffer... */
    size = sizeof "Content-Type: application/beep+xml\r\n\r\n<error code='...'" - 1;
    if (language && *language) {
        size += (sizeof " xml:lang=''" - 1) + strlen (language);
        for (cp = language; *cp; cp++)
            if ((*cp == '&') || (*cp == '\''))
               size += 5;
    }
    if (diagnostic && *diagnostic) {
        size += (sizeof "></error>" - 1) + strlen (diagnostic);
        for (cp = diagnostic; *cp; cp++)
            if ((*cp == '&') || (*cp == '<'))
               size += 4;
    } else
        size += sizeof " />" - 1;

    if (!(buffer = (channel_inst != NULL)
		       ? bpc_buffer_allocate (channel_inst, size)
		       : lib_malloc (size)))
	return NULL;
    ep = buffer;

    /* fill it in... */
    if (channel_inst != NULL) {
        strcpy (ep, "Content-Type: application/beep+xml\r\n\r\n");
	ep += strlen (ep);
    }
    sprintf (ep, "<error code='%d'", code);
    ep += strlen (ep);
    if (language && *language) {
        strcpy (ep, " xml:lang='");
        ep += strlen (ep);
        for (cp = language; *cp; cp++) {
            switch (*cp) {
                case '&':
                    strcpy (ep, "&amp;"), ep += strlen (ep);
                    break;

                case '\'':
                    strcpy (ep, "&apos;"), ep += strlen (ep);
                    break;

                default:
                    *ep++ = *cp;
                    break;
            }
        }
        *ep++ = '\'';
    }
    if (diagnostic && *diagnostic) {
        *ep++ = '>';
        for (cp = diagnostic; *cp; cp++) {
            switch (*cp) {
                case '&':
                    strcpy (ep, "&amp;"), ep += strlen (ep);
                    break;

                case '<':
                    strcpy (ep, "&lt;"), ep += strlen (ep);
                    break;

                default:
                    *ep++ = *cp;
                    break;
            }
        }
        strcpy (ep, "</error>");
        ep += strlen (ep);
    } else {
        strcpy (ep, " />");
        ep += strlen (ep);
    }

    return buffer;
}

void bpc_send(CHANNEL_INSTANCE *channel_inst, char msg_type, long msg_number,
              long ans_number, char more, char* buffer, long length)
{
    WRAPPER * wrap;
    struct frame * f;

    if(msg_number == -1)
        assert(msg_type == BLU_FRAME_TYPE_MSG);
    if(ans_number != -1)
        assert(msg_type == BLU_FRAME_TYPE_ANS);

    wrap = GET_WRAPPER(channel_inst->conn);
    if (wrap->conn.status == INST_STATUS_EXIT)
        return;

    /* walk back to the begining of the frame which owns buffer and
     * assign to frame
     */
    f = (struct frame *)(buffer - sizeof(struct frame) -
                         wrap->session->frame_pre);

    /* set the msg_type, more and paylod fields */
    f->channel_number = channel_inst->channel_number;
    f->msg_type = msg_type;
    f->message_number = msg_number;
    f->answer_number = ans_number;
    f->more = more;
    f->payload = buffer;
    f->size = length;

    SEM_WAIT(&wrap->lock);
    if (channel_inst->status == INST_STATUS_STARTING) {
        bp_queue_put(channel_inst->pending_send, f);
        SEM_POST(&wrap->lock);
    } else {
        SEM_POST(&wrap->lock);
        SEM_WAIT(&wrap->core_lock);
        blu_frame_send(f);
        SEM_POST(&wrap->core_lock);
    }

    return;
}



struct frame *bpc_query(CHANNEL_INSTANCE *inst, char msgtype, long msgno,
			 long ansno,char more)
{
    struct frame *f=NULL;
    WRAPPER * wrap;
    long channel_number;

    wrap = GET_WRAPPER(inst->conn);
    if (wrap->conn.status == INST_STATUS_EXIT)
        return f;
    channel_number = inst->channel_number;

    SEM_WAIT(&wrap->core_lock);
    f = blu_frame_query(wrap->session, channel_number, msgtype, msgno,
                        ansno, more);
    SEM_POST(&wrap->core_lock);
    return f;
}  

FRAME *bpc_query_frame(CHANNEL_INSTANCE *inst, char msgtype, 
			long msgno, long ansno)
{
    return bpc_query(inst, msgtype, msgno, ansno, BLU_QUERY_PARTIAL);
}

FRAME *bpc_query_message(CHANNEL_INSTANCE *inst, char msgtype, 
			  long msgno, long ansno)
{
    return bpc_query(inst, msgtype, msgno, ansno, BLU_QUERY_COMPLETE);
}


int bpc_frame_aggregate(CHANNEL_INSTANCE *inst, struct frame *in, char** buffer)
{
    WRAPPER * wrap;
    struct frame *f=NULL;

    wrap = GET_WRAPPER(inst->conn);
    if (wrap->conn.status == INST_STATUS_EXIT)
        return -1;

    SEM_WAIT(&wrap->core_lock);
    f = blu_frame_aggregate(wrap->session, in);
    SEM_POST(&wrap->core_lock);

    *buffer = f->payload;
    return f->size;
}

void bpc_message_destroy(CHANNEL_INSTANCE *inst, struct frame *frame)
{
    WRAPPER * wrap;
    struct frame *inframe, *outframe;    
    long msgno;
    int j;

    wrap = GET_WRAPPER(inst->conn);

    SEM_WAIT(&wrap->core_lock);
    inframe = frame;
    outframe = inframe;
    msgno = frame->message_number;

    j=1;
    do
    {
        if ((inframe->more == '.') || (msgno != inframe->message_number))
            j=0;
        outframe=inframe->next_in_message;
        blu_frame_destroy(inframe);
        inframe=outframe;
    } while (j && inframe);	    
    SEM_POST(&wrap->core_lock);

    return;
}



void bpc_buffer_destroy(CHANNEL_INSTANCE *inst,char* buffer)
{
    WRAPPER * wrap;
    struct frame *tmpframe;    

    wrap = GET_WRAPPER(inst->conn);

    SEM_WAIT(&wrap->core_lock);
    /* walk back to the frame */
    tmpframe = (struct frame *)(buffer - sizeof(struct frame) -
                                wrap->session->frame_pre);
    blu_frame_destroy(tmpframe);
    SEM_POST(&wrap->core_lock);
    return;
}


void bpc_frame_destroy(CHANNEL_INSTANCE *inst,struct frame *frame)
{
    WRAPPER * wrap;

    wrap = GET_WRAPPER(inst->conn);

    SEM_WAIT(&wrap->core_lock);
    blu_frame_destroy(frame);
    SEM_POST(&wrap->core_lock);
    return;
}

void bpc_set_channel_info(CHANNEL_INSTANCE *inst, void *data)
{
    WRAPPER * wrap = GET_WRAPPER(inst->conn);

    SEM_WAIT(&wrap->lock);
    inst->channel_info = data;
    SEM_POST(&wrap->lock);
    return;
}

    
void *bpc_get_channel_info(CHANNEL_INSTANCE *inst)
{
    void *ret;
    WRAPPER * wrap = GET_WRAPPER(inst->conn);

    SEM_WAIT(&wrap->lock);
    ret = inst->channel_info;
    SEM_POST(&wrap->lock);
    return ret;
}

long bpc_set_channel_window(CHANNEL_INSTANCE *inst, long size)
{
    long ret;
    WRAPPER * wrap;
    long channel_number;

    wrap = GET_WRAPPER(inst->conn);
    channel_number = inst->channel_number;
    SEM_WAIT(&wrap->core_lock);
    ret = blu_local_window_set(wrap->session, channel_number, size);
    SEM_POST(&wrap->core_lock);
    return ret; 
}

/*
 * PRE: acquire wrap->lock
 */
CHANNEL_INSTANCE *channel_instance_new(WRAPPER *wrap,
                                       PROFILE_REGISTRATION *reg,
                                       struct chan0_msg *icz,
                                       struct chan0_msg *ocz, 
                                       start_request_callback app_start,
                                       close_request_callback app_close,
                                       void *client_data)
{
    CHANNEL_INSTANCE *ci;

    ci = (CHANNEL_INSTANCE *)blw_malloc(wrap, sizeof(CHANNEL_INSTANCE));
    if (!ci)
        return NULL;
    memset(ci,'\0',sizeof(CHANNEL_INSTANCE));

    ci->profile =
        (PROFILE_INSTANCE *)blw_malloc(wrap, sizeof(PROFILE_INSTANCE));
    if (!ci) {
        lib_free(ci);
        return NULL;
    }
    memset(ci->profile, '\0', sizeof(PROFILE_INSTANCE));
    ci->conn = &wrap->conn;
    ci->profile->channel = ci;
    

    if (icz) {
        ci->channel_number = icz->channel_number;
        ci->status = icz->op_sc;
    }
    else if (ocz) {
        ci->channel_number = ocz->channel_number;
        ci->status = ocz->op_sc;
    }

    blw_wkr_init_profile(ci->profile, reg);
    SEM_INIT(&ci->user_sem1, 1);
    SEM_INIT(&ci->user_sem2, 1);
    SEM_WAIT(&ci->user_sem1);

    ci->outbound = ocz;
    ci->inbound = icz;
    ci->app_sr_callback = app_start;
    ci->app_cr_callback = app_close;
    ci->cb_client_data = client_data;
    ci->diagnostic = NULL;
    ci->commit = ' ';
    ci->pending_send = bp_queue_new(10);
    ci->pending_event = bp_queue_new(10);
    ci->tid=0;

    wrap->channel_ref_count++;

    return ci;
}

void channel_instance_destroy(CHANNEL_INSTANCE *inst)
{
    WRAPPER * wrap = GET_WRAPPER(inst->conn);
    DIAGNOSTIC *diag;

    wrap->channel_ref_count--;

    diag = blw_get_remembered_diagnostics(wrap,inst->channel_number,
                                          200,NULL);
    if (diag)
    {
        wrap->log(LOG_WRAP,1,"remembered diagnostic: %s",diag->message);
        blw_diagnostic_destroy(wrap,diag);
    }

    SEM_TRYWAIT(&inst->user_sem1);
    SEM_POST(&inst->user_sem1);
    SEM_TRYWAIT(&inst->user_sem2);
    SEM_POST(&inst->user_sem2);

    bp_queue_free(inst->pending_send);
    bp_queue_free(inst->pending_event);
    blw_free(wrap, inst->profile);
    blw_free(wrap, inst);
}

void bpc_wait_state_quiescent(CHANNEL_INSTANCE* channel)
{
    waitfor_chan_stat_quiescent(GET_WRAPPER(channel->conn),
                                channel->channel_number);
}

struct chan0_msg *chan0_msg_parse ();
struct diagnostic *bpc_parse_error (CHANNEL_INSTANCE *inst,
				    struct frame *f) {
    WRAPPER *wrap = GET_WRAPPER (inst -> conn);
    struct chan0_msg *msg = chan0_msg_parse (wrap -> session, f);
    DIAGNOSTIC *d = NULL;

    if (!msg)
        return NULL;

    if (msg -> error)
        d = blw_diagnostic_new (wrap, msg -> error -> code,
				msg -> error -> lang, "%s",
				msg -> error -> message);

    blw_chan0_msg_destroy (wrap, msg);

    return d;
}

struct frame *bpc_b2f (CHANNEL_INSTANCE *inst, char *buffer) {
    WRAPPER    *w = GET_WRAPPER (inst -> conn);

    return ((FRAME *) (buffer - sizeof (struct frame)
		             - w -> session -> frame_pre));
}
