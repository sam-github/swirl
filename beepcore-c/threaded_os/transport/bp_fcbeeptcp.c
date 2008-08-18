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
#ifndef WIN32
#include <sys/types.h>
#endif
#include "../../core/generic/CBEEP.h"
#include "../../core/generic/CBEEPint.h"
#include "../utility/logutil.h"
#include "../utility/semutex.h"
#include "../wrapper/bp_wrapper.h"
#include "../wrapper/bp_wrapper_private.h"
#include "bp_fpollmgr.h"

sem_t upper_sem;

extern int blw_put_remembered_error(WRAPPER *wrap, long channel_number, 
                                    DIAGNOSTIC *diag);

void notify_lower(struct session * s, int i) 
{
    IO_STATE *thisio=NULL;
    WRAPPER *wrap;
#ifdef DEBUGFORK
    char *cp;
#endif
    int err;
    long channel;
    DIAGNOSTIC *diag;

    SEM_WAIT(&upper_sem);
    wrap = (WRAPPER *)blu_session_info_get(s);
    if (wrap)
    {
        thisio = wrap->iostate;
        if (!wrap->session)
            thisio = NULL;
        if (thisio)
            if (!thisio->pn)
                thisio=NULL;
    }
    if (thisio)
    {
        switch(i)
        {
            case 1:
                
                err = bll_status(s);
                if (err)
                {
                    wrap->log(LOG_WRAP,4,"lower error: %s",bll_status_text(s));
                    switch(err)
                    {
                        case 1: /* tuning reset complete */
                        case 2: /* Session closed cleanly */
                        case 3: /* <error> instead of greeting received */
                        case 4: /* Reserved */
                        case 5: /* Out of memory, non-recoverable */
                        case 6: /* Other resource error */
                            channel = bll_status_channel(s);
                            if (channel < 0)
                                channel=0;
                            if (wrap->error_remembered)
                            {
                                diag = blw_diagnostic_new(wrap, err, NULL,
							  "%s",
                                                          bll_status_text(s));
                                blw_put_remembered_error(wrap, channel,diag);
                            }
                            wrap->conn.status = INST_STATUS_EXIT;
                            fiostate_stop(thisio);
                            break;
                        case -3: /* Query for complete message with full but incomplete buffer */
                        case 7:  /* Local profile error */
                        case -4: /* Ignorable local error */
                            channel = bll_status_channel(s);
                            if (channel < 0)
                                channel=0;
                            if (wrap->error_remembered)
                            {
                                diag = blw_diagnostic_new(wrap, err, NULL,
							  "%s",
                                                          bll_status_text(s));
                                blw_put_remembered_error(wrap,channel,diag);
                            }
                            printf("Unexpected error(%d) %s\n", err,
                                   bll_status_text(s));
                            FAULT(s,0,NULL);
                            break;
                        case -1: /* Out of memory, recoverable. */
                        case -2: /* Tried to exceed set memory limits. */
                        case 8:
                            channel = bll_status_channel(s);
                            if (channel < 0)
                                channel=0;
                            diag = blw_diagnostic_new(wrap, err, NULL, "%s",
                                                      bll_status_text(s));
                            wrap->conn.status = INST_STATUS_EXIT;
                            blw_put_remembered_error(wrap,channel,diag);
                            fiostate_stop(thisio);
                            break;
                    }
                }
                break;
            case 2:
                fiostate_unblock_read(thisio);
                break;
            case 3:
                SEM_POST(&thisio->lock);
                if (thisio->state >= IOS_STATE_ACTIVE)
                    thisio->state=IOS_STATE_WRITE_PENDING;
                SEM_POST(&thisio->lock);
                fiostate_unblock_write(thisio);
                break;
            default:
                /* handle status changes */
#ifdef DEBUGFORK
                printf("UNKNOWN lower error %d\n",i); 
#endif
                err = bll_status(s);
                ASSERT(err == 0);
                if (err)
                    fpollmgr_shutdown();
                break;
        }
    }
    SEM_POST(&upper_sem);
#ifdef DEBUGFORK
    printf("Notify lower: %d\n", i);
    cp = bll_status_text(s);
    if (cp==NULL) cp = ">>NULL<<";
    printf("  text: %s\n", cp);
    printf("  chan: %ld\n", bll_status_channel(s)); 
#endif


}
