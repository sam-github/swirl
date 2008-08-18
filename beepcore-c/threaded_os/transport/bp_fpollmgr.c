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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/uio.h>
#include <errno.h>
#endif
#include "../utility/semutex.h"
#include "../../utility/bp_slist_utility.h"
#include "bp_fpollmgr.h"
#include "bp_fiostate.h"
#include "../wrapper/bp_wrapper.h"
#include "../wrapper/bp_wrapper_private.h"
#include "../utility/logutil.h"
#include "../../core/generic/CBEEP.h"
#include "../../core/generic/CBEEPint.h"

extern int handle_read_event(IO_STATE* ios);
extern void handle_write_event(IO_STATE* ios);
extern void handle_socket_error(IO_STATE* ios);

extern sem_t upper_sem;

static unsigned int Max_polls;

static sem_t poll_list_lock;
static bp_slist* poll_list = NULL;
static unsigned int max_connections = 0;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !(FALSE)
#endif

void poll_node_remove(POLL_NODE* pn);

void checkdown(POLL_NODE* pn)
{
    unsigned int i;
    IO_STATE *ios;

    for (i=0; i<pn->size; ++i) {
        ios = pn->iostateindex[i];
        if (ios != NULL && ios->shutdown == 1) {
            if (ios->iostate_index < pn->count - 1) {
                IO_STATE* ios2 = pn->iostateindex[pn->count - 1];

                SEM_WAIT(&ios2->lock);

                pn->iostateindex[ios->iostate_index] = ios2;
                ios2->iostate_index = ios->iostate_index;
                ios->iostate_index = pn->count - 1;

#ifndef WIN32
                memcpy(&pn->sessionfds[ios2->iostate_index],
                       &pn->sessionfds[ios->iostate_index],
                       sizeof(pn->sessionfds[0]));
#endif

                SEM_POST(&ios2->lock);
            }

            ios->shutdown = 2;
            pn->iostateindex[ios->iostate_index] = NULL;
            ios->iostate_index = -1;
#ifndef WIN32
            pn->sessionfds[ios->iostate_index].events = 0;
            pn->sessionfds[ios->iostate_index].fd = DIO;
            pn->sessionfds[ios->iostate_index].revents = 0;
#endif
            pn->count--;
            /* if the shutdown was not initiated here notify the wrapper */
            if (ios->wrapper) {
                shutdown_callback(ios->wrapper);
            }
        }
    }
}
   
/*
 * ios->state
 *      0 connected
 *      1 the first write goes out (greeting)
 *      2 not used (used to test SEQs go out in the right order)
 *      3 up and running
 *      4 while writing another write event happened (from notify_lower)
 */
#ifdef WIN32
DWORD WINAPI IW_fpollmgr_fds(void *data)
{
    POLL_NODE* cons = (POLL_NODE*)data;
    /* The number of sockets in exceptFds since read and write can be blocked */
    unsigned int i;
    unsigned int count;

    fd_set readFds;
    fd_set writeFds;
    fd_set exceptFds;

    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);

    while (TRUE) {
        int n;
        struct timeval timeout = {0, 500};
        
        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);
        FD_ZERO(&exceptFds);
        
        SEM_WAIT(&cons->lock);
        if (cons->count <= 0) {
            cons->shutdown = TRUE;
        }
        if (cons->shutdown == TRUE) {
            cons->threadHandle = NULL;
            cons->tid = -1;
            SEM_POST(&cons->lock);
            break;
        }

        for (i=0, count=0; i<cons->size && count<cons->count; ++i) {
            if (cons->iostateindex[i] != NULL &&
                cons->iostateindex[i]->state != IOS_STATE_INITIALIZING)
            {

                if ((cons->iostateindex[i]->events &
                     (IOS_EVENT_READ | IOS_EVENT_WRITE)) != 0)
                {
                    if (cons->iostateindex[i]->events & IOS_EVENT_READ) {
                        FD_SET(cons->iostateindex[i]->socket, &readFds);
                    }
                    if (cons->iostateindex[i]->events & IOS_EVENT_WRITE) {
                        FD_SET(cons->iostateindex[i]->socket, &writeFds);
                    }
                    FD_SET(cons->iostateindex[i]->socket, &exceptFds);
                    ++count;
                }
            }
        }
        SEM_POST(&cons->lock);

        if (count == 0) {
            checkdown(cons);
            continue;
        }

        n = select(count, &readFds, &writeFds, &exceptFds, &timeout);

        if (cons->shutdown == TRUE)
            break;
        
        if (n == SOCKET_ERROR) {
            int rc = WSAGetLastError();
        }
        if (n <= 0) {
            checkdown(cons);
            continue;
        }
        
        for (i=0; i<cons->count; ++i) {
            WRAPPER *wrap;
            READER_WRITER *wrapio;
            IO_STATE* ios = cons->iostateindex[i];

            if (ios->shutdown != 0) {
                continue;
            }
            wrap = ios->wrapper;
            if (wrap == NULL) {
                /*
                 * the wrapper has closed the connection the ios just needs
                 * to be cleaned up.
                 */
                continue;
            }
            wrapio = (READER_WRITER *)ios->stack_reader_writer->data;

            if (FD_ISSET(ios->socket, &readFds) == FALSE) {
                /* Add this socket back for the next select */
                if (ios->events & IOS_EVENT_READ) {
                    FD_SET(ios->socket, &readFds);
                }
            } else if (ios->events & IOS_EVENT_READ) {
                if (handle_read_event(ios) == -1) {
                    wrap->log(LOG_WRAP,2, "%d %d hung up", i, ios->socket);
                    handle_socket_error(ios);
                }
            }

            /*
            * POLLOUT is requested by notify_lower when there is
            * data to write
            */
            if (FD_ISSET(ios->socket, &writeFds) == FALSE) {
                /* Add this socket back for the next select */
                if (ios->events & IOS_EVENT_WRITE) {
                    FD_SET(ios->socket, &readFds);
                }
            } else if (ios->events & IOS_EVENT_WRITE) {
                handle_write_event(ios);
            }

            if (FD_ISSET(ios->socket, &exceptFds)) {
                wrap->log(LOG_WRAP,2, "%d %d hung up", i, ios->socket);
                handle_socket_error(ios);
                continue;
            }
        }
        checkdown(cons);
    }
    return 0;
}
#else
void *IW_fpollmgr_fds(void *data)
{
    POLL_NODE *pn = (POLL_NODE *)data;
    short revents;
    int i,ioevent=0;
    IO_STATE *ios;

    while (TRUE) {
        YIELD();
        SEM_WAIT(&pn->lock);
        if (pn->count <= 0) {
            pn->shutdown = TRUE;
        }
        if (pn->shutdown == TRUE) {
            pn->tid = -1;
            SEM_POST(&pn->lock); 
            break;
        }
        ioevent = poll(pn->sessionfds, pn->size, pn->size * 3);
        SEM_POST(&pn->lock); 
        while (ioevent > 0)
        {
            for (i=0;i<pn->size;i++)
            {
                if (pn->sessionfds[i].fd == DIO)
                    continue;
                revents = pn->sessionfds[i].revents;
                if (revents)
                {
                    ios = pn->iostateindex[i];
                    if (!ios)
                    {
                        ioevent--;
                        continue;
                    }
                    /*
                     * POLLOUT is requested by notify_lower when there is
                     * data to write
                     */
                    if (((revents & POLLOUT) == POLLOUT))
                    {
                        handle_write_event(ios);
                        if (ios->state != IOS_STATE_WRITE_PENDING) {
                            ioevent--;
                        }
                        continue;
                    }
                    if (((revents & POLLIN) == POLLIN))
                    {
                        if (handle_read_event(ios) != 0) {
                            revents = revents | POLLHUP;
                        }
                        ioevent--;
#if 0
                        if ((revents & POLLHUP) != POLLHUP)
                        {
                            continue;
                        }
#endif
                    }
                    if ((revents & (POLLHUP | POLLERR | POLLNVAL)))
                    {
                        ios->wrapper->log(LOG_WRAP, 2,
                                          "id=%d fd=%d revents=0x%x/0x%x",
                                          pn -> poll_id,
                                          pn -> sessionfds[i].fd,
                                          revents,
                                          pn -> sessionfds[i].revents);
                        handle_socket_error(ios);
                        continue;
                    }
                }
            }
        }
        checkdown(pn);
    }
    return 0;
}
#endif

POLL_NODE* poll_node_alloc(int size)
{
    int i;
    POLL_NODE* pn;

    pn = (POLL_NODE *) malloc(sizeof(POLL_NODE));
    if (pn == NULL) {
        return NULL;
    }

    SEM_INIT(&pn->lock, 1);
    pn->iostateindex = (IO_STATE **) malloc(size * sizeof(IO_STATE *));
    if (pn->iostateindex == NULL) {
        SEM_DESTROY(&pn->lock);
        return NULL;
    }

    for (i=0; i<size; ++i) {
        pn->iostateindex[i] = NULL;
    }
    pn->size = size;
    pn->count = 0;
#ifndef WIN32
    pn->sessionfds = (struct pollfd *) malloc(size * sizeof(struct pollfd));
    if (pn->sessionfds == NULL) {
        free(pn->iostateindex);
        SEM_DESTROY(&pn->lock);
        return NULL;
    }
    for (i=0; i<size; ++i) {
        pn->sessionfds[i].fd = DIO;
        pn->sessionfds[i].events = 0;
        pn->sessionfds[i].revents = 0;
    }
#endif
    pn->shutdown = TRUE;

    return pn;
}

void poll_node_free(POLL_NODE* pn)
{
#if 0
#ifndef WIN32
    free(pn->sessionfds);
#endif
    free(pn->iostateindex);
#endif
    SEM_DESTROY(&pn->lock);
    free(pn);
}

POLL_NODE* poll_node_add(void)
{
    POLL_NODE *pn;

    ASSERT(max_connections != 0);

    if (bp_slist__length(poll_list) == MAX_POLL_THREADS) {
        return NULL;
    }

    pn = poll_node_alloc(max_connections);
    if (pn == NULL) {
        return NULL;
    }

    pn->poll_id = bp_slist__length(poll_list);

    poll_list = bp_slist__append(poll_list, pn);

    return pn;
}

void poll_node_remove(POLL_NODE* pn)
{
    SEM_WAIT(&poll_list_lock);
    SEM_WAIT(&pn->lock); 
    if (pn->count == 0) {
        poll_list = bp_slist__remove(poll_list, pn);
        SEM_POST(&pn->lock); 
        poll_node_free(pn);
    } else {
        SEM_POST(&pn->lock); 
    }
    SEM_POST(&poll_list_lock);
}

POLL_NODE* poll_node_select()
{
    POLL_NODE *pn;
    int length;
    int i;

    length = bp_slist__length(poll_list);
    for (i=0; i<length; ++i) {
        pn = bp_slist__index_data(poll_list, i);
        if (pn->count < pn->size) {
            return pn;
        }
    }
    return NULL;
}

int poll_node_available_index(POLL_NODE *pn)
{
    ASSERT(pn->count < pn->size);

    return pn->count;
}

int poll_node_add_iostate(IO_STATE* ios)
{
    POLL_NODE *pn;
    int i; 

    SEM_WAIT(&poll_list_lock);
    pn = poll_node_select();
    if (pn == NULL) {
        pn = poll_node_add();
        if (pn == NULL) {
            SEM_POST(&poll_list_lock);
            return -1;
        }
    }
    SEM_WAIT(&pn->lock);
    SEM_POST(&poll_list_lock);
    i = poll_node_available_index(pn);

    pn->count++;
    pn->iostateindex[i] = ios;
#ifndef WIN32
    pn->sessionfds[i].fd = ios->socket;
#endif
    ios->iostate_index = i;
    ios->pn = pn;
    if (pn->shutdown == TRUE) {
        pn->shutdown = FALSE;
#ifdef WIN32
        pn->threadHandle = CreateThread(NULL, 0, IW_fpollmgr_fds, pn, 0,
                                        &pn->tid);
        ASSERT(pn->threadHandle != NULL);
#else
        THR_CREATE(&pn->tid, NULL, IW_fpollmgr_fds, pn);
#endif
    }
    SEM_POST(&pn->lock);

    return 0;
}

int fpollmgr_init(int maxConnections)
{
    ASSERT(max_connections == 0);

    SEM_INIT(&poll_list_lock, 1);
    SEM_INIT(&upper_sem, 1);

    max_connections = maxConnections;

    return 0;
}

int fpollmgr_shutdown()
{
    unsigned int i;
    unsigned int length;
    POLL_NODE *pn;
#ifndef WIN32
    void *tidret;
#endif

    ASSERT(max_connections != 0);

    if (Max_polls == (-1))
        return -1;

    SEM_WAIT(&poll_list_lock);
    length = bp_slist__length(poll_list);
    for (i=0; i<length; ++i) {
        pn = (POLL_NODE *)bp_slist__index_data(poll_list, i);
        pn->shutdown = TRUE;
#ifdef WIN32
        WaitForSingleObject(pn->threadHandle, INFINITE);
#else
        THR_JOIN(pn->tid, &tidret);
#endif
        for (i=0; i<pn->size; ++i) {
            if (pn->iostateindex[i] == NULL)
                continue;
            pn->iostateindex[i]->shutdown = 1;
            fiostate_stop(pn->iostateindex[i]);
        }
        checkdown(pn);
        poll_node_free(pn);
    }
    bp_slist__free(poll_list);
    poll_list = NULL;
    SEM_POST(&poll_list_lock);

    SEM_DESTROY(&upper_sem);
    SEM_DESTROY(&poll_list_lock);

    Max_polls = (-1);

    max_connections = 0;
    return 0;
}

/* 
   determine the maximum socket descriptors this user can 
   system poll
*/

int IW_fpoll_max(int max_files)
{
#ifdef WIN32
    return max_files;
#else
   struct pollfd *pfds;
   int i;
   int j;

   if (!(pfds = (struct pollfd *)malloc(max_files*sizeof(struct pollfd))))
       return max_files;

   for (i=0;i<max_files;i++)
   {
       pfds[i].fd = 1;
       pfds[i].events = POLLOUT;
   }

   i=1;
   while (i<=max_files && (j=poll(pfds, i, 0)) >= 0)
       i *=2;
   i=i/2;
   while (i<=max_files && (j=poll(pfds, i, 0)) >= 0)
       i++;
   free(pfds);
   i--;

#ifdef	UNPROVEN
   if (i > FD_SETSIZE)
       i = FD_SETSIZE;
#endif

   return i;
#endif
}
