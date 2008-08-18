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
 * $Id: workthread_win32.c,v 1.2 2001/10/30 06:00:38 brucem Exp $
 *
 * IW_workthread.c
 *
 */
#ifndef WIN32
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#else
#include <windows.h>
#endif
#include <stdio.h>
#include <string.h>
#include "../../utility/bp_malloc.h"
#include "../../utility/bp_queue.h"
#include "../../utility/bp_slist_utility.h"
#include "workthread.h"


#ifdef WIN32
HANDLE sem_thread_start;
HANDLE sem_thread_work;
#else
sem_t sem_thread_start;
sem_t sem_thread_work;
#endif

IW_WORK_THREAD *IW_work_thread_init();
bp_slist *work_thread_list=NULL;

#ifdef WIN32
DWORD WINAPI IW_work_thread_start(void *data);
#endif

int IW_work_thread_begin()
{
#ifdef WIN32
    sem_thread_start = CreateSemaphore(NULL, 1, 1, NULL);
    if (sem_thread_start == NULL) {
        return -1;
    }

    sem_thread_work = CreateMutex(NULL, FALSE, NULL);
    if (sem_thread_start == NULL) {
        return -1;
    }
#else
    sem_init(&sem_thread_start,0,1);
    sem_init(&sem_thread_work,0,1);
#endif

    return 0;
}

IW_WORK_THREAD *IW_work_thread_init()
{
    IW_WORK_THREAD *work_thread;

#ifdef WIN32
    if (WaitForSingleObject(sem_thread_start, INFINITE) == WAIT_FAILED) {
        return NULL;
    }
#else
    sem_wait(&sem_thread_start);    
#endif

    work_thread = (IW_WORK_THREAD *)lib_malloc(sizeof(IW_WORK_THREAD));
    if (!work_thread)
        return NULL;
    work_thread->tid = 0;
    work_thread->used = 0;
    work_thread->work_queue = bp_queue_new(WORK_QUEUE_SIZE);

#ifdef WIN32
    if (WaitForSingleObject(sem_thread_work, INFINITE) == WAIT_FAILED) {
        return NULL;
    }
#else
    sem_wait(&sem_thread_work);
#endif

    work_thread_list = bp_slist__prepend(work_thread_list,work_thread);

#ifdef WIN32
    ReleaseMutex(sem_thread_work);
#else
    sem_post(&sem_thread_work);
#endif

    return work_thread;
}


IW_WORK_THREAD *IW_work_thread_new()
{
    IW_WORK_THREAD *work_thread;
    int work_threads_used;

    work_threads_used = bp_slist__length(work_thread_list);
    if (work_threads_used > MAX_WORK_THREAD)
        return NULL;

    work_thread = IW_work_thread_init();
    if (!work_thread)
        return NULL;
#ifdef WIN32
    work_thread->handle = CreateThread(NULL, 0, IW_work_thread_start,
        work_thread, 0, &work_thread->tid);
    if (work_thread->handle == NULL)
        IW_work_thread_destroy(&work_thread);
#else
    err = pthread_create(&work_thread->tid,NULL,IW_work_thread_start,
        (void *)work_thread);
    if (err)
        IW_work_thread_destroy(&work_thread);
    else
        pthread_detach(work_thread->tid);
#endif
    return work_thread;
}

#ifdef WIN32
DWORD WINAPI IW_work_thread_start(void *data)
#else
void *IW_work_thread_start(void *data)
#endif
{

    IW_WORK_THREAD *work_thread;
    WORKTHREAD_Q_REC *qrec=NULL;
    int forever = 1,err;

    work_thread = (IW_WORK_THREAD *)data;
#ifdef WIN32
    work_thread->sem_work_wait = CreateSemaphore(NULL, 0, 1024, NULL);
    if (work_thread->sem_work_wait == NULL) {
        ReleaseSemaphore(sem_thread_start, 1, NULL);
        return GetLastError();
    }
    work_thread->sem_work_queue = CreateMutex(NULL, FALSE, NULL);
    if (work_thread->sem_work_queue == NULL) {
        ReleaseSemaphore(sem_thread_start, 1, NULL);
        return GetLastError();
    }
#else
    sem_init(&work_thread->sem_work_wait,0,0);
//    sem_init(&work_thread->sem_work_wait,0,1);
//    sem_wait(&work_thread->sem_work_wait);
    sem_init(&work_thread->sem_work_queue,0,1);
#endif

#ifdef WIN32
    ReleaseSemaphore(sem_thread_start, 1, NULL);
#else
    sem_post(&sem_thread_start);
#endif
    do
    {
#ifdef WIN32
        if (WaitForSingleObject(work_thread->sem_work_wait, INFINITE) ==
            WAIT_FAILED)
        {
            continue;
        }
#else
        sem_wait(&work_thread->sem_work_wait);
#endif
        do
        {
#ifdef WIN32
            if (WaitForSingleObject(work_thread->sem_work_queue, INFINITE) ==
                WAIT_FAILED)
            {
                continue;
            }
#else
            sem_wait(&work_thread->sem_work_queue);
#endif
            if (work_thread && work_thread->work_queue)
            {
		qrec = (WORKTHREAD_Q_REC *)bp_queue_get(work_thread->work_queue);
            }
            else
            {
                qrec = NULL;
            }	
#ifdef WIN32
            ReleaseMutex(work_thread->sem_work_queue);
#else
            sem_post(&work_thread->sem_work_queue);
#endif
            if (qrec)
            {
                if (qrec->cbfunc)
                    err = qrec->cbfunc(qrec->ClientData);
		if (qrec->ClientDataFree)
		    qrec->ClientDataFree(qrec->ClientData);
                lib_free(qrec);
            }
        } while (qrec);
        if (!work_thread)
            forever=0;
        else
            if (!work_thread->work_queue)
                forever=0;
    } while (forever);
#ifdef WIN32
    CloseHandle(work_thread->sem_work_queue);
    CloseHandle(work_thread->sem_work_wait);
#endif
    /*    work_thread->tid=0; */
#ifdef WIN32
    return ERROR_SUCCESS;
#else
    return (void *)NULL;
#endif
}



void IW_work_thread_destroy(IW_WORK_THREAD **work_thread)
{
    IW_WORK_THREAD *wt; 
    WORKTHREAD_Q_REC *qrec;
#ifndef WIN32
    void *tidret;
#endif

    if (work_thread)
    {
       if (*work_thread)
       {
           work_thread_list = bp_slist__remove(work_thread_list,*work_thread);
           wt=*work_thread;
#ifdef WIN32
           if (WaitForSingleObject(wt->sem_work_queue, INFINITE) ==
               WAIT_FAILED)
           {
           }
#else
           sem_wait(&wt->sem_work_queue);
#endif
           do
           {
               qrec = (WORKTHREAD_Q_REC *)bp_queue_get(wt->work_queue);
               if (qrec)
		   if (qrec->ClientDataFree)
		       qrec->ClientDataFree(qrec->ClientData);
                   lib_free(qrec);
           } while (qrec);
           bp_queue_free(wt->work_queue);
           wt->work_queue = NULL;
#ifdef WIN32
           ReleaseMutex(wt->sem_work_queue);
           WaitForSingleObject(wt->sem_work_wait, 0);
           ReleaseMutex(wt->sem_work_wait);
           WaitForSingleObject(wt->handle, INFINITE);
           CloseHandle(wt->sem_work_wait);
           CloseHandle(wt->sem_work_queue);
#else
           sem_post(&wt->sem_work_queue);
           sem_trywait(&wt->sem_work_wait);
           sem_post(&wt->sem_work_wait);
           pthread_join(wt->tid,&tidret);
           sem_destroy(&wt->sem_work_wait);
           sem_destroy(&wt->sem_work_queue);
#endif
           lib_free((IW_WORK_THREAD *)wt); 
       }
       work_thread = NULL;
    }
    return;
}

int IW_work_thread_fin()
{
    int err,i,len;
    bp_slist *list;
    IW_WORK_THREAD *wt;

    len = bp_slist__length(work_thread_list);
    for (i=0;i<len;i++)
    {
        list = bp_slist__index(work_thread_list,i);
        wt = (IW_WORK_THREAD *)list->data;
#ifdef WIN32
        WaitForSingleObject(wt->handle, INFINITE);
        err = 0;
#else
        err = pthread_cancel(wt->tid);
#endif
    }
#ifdef WIN32
    Sleep(0);
#else
    sched_yield();
#endif
    len = bp_slist__length(work_thread_list);
    while (len)
    {
        list = bp_slist__index(work_thread_list,0);
        wt = (IW_WORK_THREAD *)list->data;
        IW_work_thread_destroy(&wt);
        len = bp_slist__length(work_thread_list);
    }
    bp_slist__free(work_thread_list);
    work_thread_list = NULL;
    return err;
}


int IW_work_thread_queue(IW_WORK_THREAD *work_thread, NOTIFYCALL cbfunc,
			 void *ClientData, CLIENTDATAFREE ClientDataFree)
{
    int err;
    WORKTHREAD_Q_REC *qrec;

    
    if ((qrec = lib_malloc(sizeof(WORKTHREAD_Q_REC))))
    {
	qrec->cbfunc = cbfunc;
	qrec->ClientData = ClientData;
	qrec->ClientDataFree = ClientDataFree;

#ifdef WIN32
        if (WaitForSingleObject(work_thread->sem_work_queue, INFINITE) ==
            WAIT_FAILED)
        {
        }
#else
	sem_wait(&work_thread->sem_work_queue);
#endif
	err = bp_queue_put(work_thread->work_queue, qrec);
	if (err)
	    {
		bp_queue_grow(work_thread->work_queue,WORK_QUEUE_SIZE);
		err = bp_queue_put(work_thread->work_queue, qrec);
	    }
#ifdef WIN32
        ReleaseMutex(work_thread->sem_work_queue);
#else
        sem_post(&work_thread->sem_work_queue);
#endif
    } else {
	err = -1;
    }

    if (!err)
    {
#ifdef WIN32
        WaitForSingleObject(work_thread->sem_work_wait, 0);
        ReleaseSemaphore(work_thread->sem_work_wait, 1, NULL);
#else
        sem_trywait(&work_thread->sem_work_wait);
        sem_post(&work_thread->sem_work_wait);
#endif
    }
    return err;
}


IW_WORK_THREAD *IW_work_thread_get()
{
    IW_WORK_THREAD *work_thread;
    bp_slist *list;
    int len,i,found;

    list = work_thread_list;

    found = 0;
    do
    {
        len = bp_slist__length(work_thread_list);
        for (i=0;i<len;i++)
        {
            list = bp_slist__index(work_thread_list,i);
            work_thread = (IW_WORK_THREAD *)list->data;
#ifdef WIN32
            if (WaitForSingleObject(sem_thread_work, INFINITE) ==
                WAIT_FAILED)
            {
                return NULL;
            }
#else
            sem_wait(&sem_thread_work);
#endif
            if (!work_thread->used)
            {
                work_thread->used = 1;
#ifdef WIN32
                ReleaseMutex(sem_thread_work);
#else
                sem_post(&sem_thread_work);
#endif
                return work_thread;
            }
#ifdef WIN32
            ReleaseMutex(sem_thread_work);
#else
            sem_post(&sem_thread_work);
#endif
        }
        work_thread = IW_work_thread_new();
    } while (work_thread);
    return NULL;
}


int IW_work_thread_mark_unused(IW_WORK_THREAD *wt)
{
#ifdef WIN32
    if (WaitForSingleObject(sem_thread_work, INFINITE) == WAIT_FAILED) {
        return -1;
    }
#else
    sem_wait(&sem_thread_work);
#endif
    wt->used = 0;
    bp_queue_clear(wt->work_queue);
#ifdef WIN32
    ReleaseMutex(sem_thread_work);
#else
    sem_post(&sem_thread_work);
#endif
    return 0;
}

void work_thread_startupwait(void) {
    WaitForSingleObject(sem_thread_work, INFINITE);
    ReleaseMutex(sem_thread_work);
}
