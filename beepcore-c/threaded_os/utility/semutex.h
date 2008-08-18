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
#ifndef	SEMUTEX_H
#define	SEMUTEX_H	1


#ifdef WIN32

#include <windows.h>
#define sem_t HANDLE
#define SEM_INIT(_sem, _count) \
            do { (*(_sem)) = CreateSemaphore(NULL, _count, 1, NULL);} while (0);

#define SEM_DESTROY(_sem) CloseHandle(*(_sem))
#define SEM_WAIT(_sem) WaitForSingleObject(*(_sem), INFINITE)
#define SEM_TRYWAIT(_sem) WaitForSingleObject(*(_sem), 0)
#define SEM_POST(_sem) ReleaseSemaphore(*(_sem), 1, NULL)
#define YIELD() Sleep(0)

#elif SUNOS

#include <thread.h>
#include <sched.h>
#include <synch.h>

#define THREAD_T thread_t 
#define sem_t sema_t
#define SEM_INIT(_sem, _count) sema_init(_sem,_count,USYNC_PROCESS,NULL)
#define SEM_WAIT(_sem) sema_wait(_sem)
#define SEM_TRYWAIT(_sem) sema_trywait(_sem)
#define SEM_POST(_sem) sema_post(_sem)
#define SEM_DESTROY(_sem) sema_destroy(_sem)
#define YIELD() thr_yield()
#define THR_CREATE(_thr,_thr_attr,_start_routine,_arg) thr_create(NULL,NULL,_start_routine,_arg,THR_BOUND,_thr)
#define THR_DETACH(_thr,thr_attr,_start_routine,_arg) thr_create(NULL,NULL,_start_routine,_arg,THR_DETACHED,_thr)
#define THR_EXIT(_exit) thr_exit(_exit)
#define THR_JOIN(_thr,_ret) thr_join(_thr,NULL,_ret)
#define THR_CANCEL(_thr)
#define THR_TESTCANCEL()

#elif LINUX

#include <pthread.h>
#include <semaphore.h>

#define THREAD_T pthread_t
#define SEM_INIT(_sem, _count) sem_init(_sem, 0, _count)
#define SEM_WAIT(_sem) sem_wait(_sem)
#define SEM_TRYWAIT(_sem) sem_trywait(_sem)
#define SEM_POST(_sem) sem_post(_sem)
#define SEM_DESTROY(_sem) sem_destroy(_sem)
#define YIELD() sched_yield()
#define THR_CREATE(_thr,_thr_attr,_start_routine,_arg) pthread_create(_thr,_thr_attr,_start_routine,_arg)
#define THR_DETACH(_thr) pthread_detach(_thr)
#define THR_EXIT(_exit) pthread_exit(_exit)
#define THR_JOIN(_thr,_ret) pthread_join(_thr,_ret)
#define THR_CANCEL(_thr) pthread_cancel(_thr)
#define THR_TESTCANCEL() pthread_testcancel()

#elif	UNPROVEN

#include <pthread.h>


/**
 * @name Semaphore structure and functions
 **/

//@{

typedef struct {
    pthread_mutex_t sem_mutex;
    pthread_cond_t sem_cond;
    int		   sem_count;
} sem_t;


/**
 * Initialize a semaphore.
 *
 * @param _sem A pointer to the semaphore structure.
 * @param _count The number of seats available.
 **/
#define SEM_INIT(_sem, _count) \
sem_init(_sem,_count,#_sem,__FILE__,__LINE__)

/**
 * Wait for a seat to free in a semaphore.
 *
 * @param _sem A pointer to the semaphore structure.
 **/
#define SEM_WAIT(_sem) \
sem_wait (_sem,#_sem,__FILE__,__LINE__)

/**
 * See if a seat is available, and if so take it.
 *
 * @param _sem A pointer to the semaphore structure.
 *
 * @return 1 success, 0 failure.
 **/
#define SEM_TRYWAIT(_sem) \
sem_trywait (_sem,#_sem,__FILE__,__LINE__)

/**
 * Add another seat to the semaphore.
 *
 * @param _sem A pointer to the semaphore structure.
 **/
#define SEM_POST(_sem) \
sem_post (_sem,#_sem,__FILE__,__LINE__)

/**
 * Reap a semaphore structure.
 *
 * @param _sem A pointer to the semaphore structure.
 **/
#define SEM_DESTROY(_sem) \
sem_destroy (_sem,#_sem,__FILE__,__LINE__)

extern	int	sem_init (sem_t *s, int c, char *what, char *file, int lineno);
extern	int	sem_wait (sem_t *s, char *what, char *file, int lineno);
extern	int	sem_trywait (sem_t *s, char *what, char *file, int lineno);
extern	int	sem_post (sem_t *s, char *what, char *file, int lineno);
extern	int	sem_destroy (sem_t *s, char *what, char *file, int lineno);

//@}

/**
 * @name Thread structure and functions
 **/

//@{

typedef pthread_t 	THREAD_T;


/**
 * Create a thread.
 *
 * @param _thr <b>(out)</b> A thread pointer updated to point to a 
 *		thread structure.
 *
 * @param _thr_attr A pointer to the thread attribute structure.
 *
 * @param _start_routine The routine to start the thread at.
 *
 * @param _arg The argument to pass to the routine.
 *
 * @return <b>0</b> or <i>errno</i>.
 **/
#define THR_CREATE(_thr,_thr_attr,_start_routine,_arg) \
pthread_create ((_thr), (pthread_attr_t *)_thr_attr, _start_routine, _arg)

/**
 * Terminate the current thread.
 *
 * @param _exit A <i>void</i> pointer to the thread's exit value.
 *
 * @return NEVER.
 **/
#define THR_EXIT(_exit) \
pthread_exit (_exit)

/**
 * Wait for a thread to terminate.
 *
 * @param _thr A pointer to the thread structure indicating the thread to
 *		wait for.
 *
 * @param _ret <b>(out)</b> A pointer updated to reflect the exit value of the
 *		terminated thread.
 **/
#define THR_JOIN(_thr,_ret) \
pthread_join (_thr,_ret)

/**
 * Yield control back to the thread scheduler.
 **/
#define	YIELD() \
pthread_yield ()

//@}

#else /* Just error since someone didn't define something that we needed. */

#error you must specify a thread platform via a define

#endif


#endif
