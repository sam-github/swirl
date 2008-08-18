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
 * $Id: workthread.c,v 1.7 2002/03/27 08:53:40 huston Exp $
 *
 * workthread.c
 *
 */
#include "../../utility/bp_malloc.h"
#include "../../utility/bp_queue.h"
#include "../../core/generic/CBEEPint.h"
#include "semutex.h"
#include "workthread.h"

#define TRUE 1

#define STATIC

#define QUEUE_INITIAL_SIZE 10
#define QUEUE_GROW_AMOUNT  5
#define STATE_INIT         1
#define STATE_EXITING      2

struct _WORKQUEUE {
    sem_t lock;
    sem_t cond;
    int state;
    int thread_count;
    int thread_available;
    bp_queue* work_items;
};

typedef struct {
    void (*func)(void* data);
    void* data;
} WORKITEM;

WORKQUEUE* workqueue_create()
{
    WORKQUEUE* queue = lib_malloc(sizeof(WORKQUEUE));
    if (queue == NULL) {
        return NULL;
    }

    SEM_INIT(&queue->lock, 1);
    SEM_INIT(&queue->cond, 0);
    queue->state = STATE_INIT;
    queue->thread_count = 0;
    queue->thread_available = 0;
    queue->work_items = bp_queue_new(QUEUE_INITIAL_SIZE);

    return queue;
}

void workqueue_destroy(WORKQUEUE* queue)
{
    int i;

    SEM_WAIT(&queue->lock);
    ASSERT(queue->state == STATE_INIT);
    ASSERT(queue->work_items->entries == 0);
    queue->state = STATE_EXITING;
    SEM_POST(&queue->lock);

    for (i=0; i<queue->thread_count; i++) {
        SEM_POST(&queue->cond);
    }

    while (queue->thread_count != 0) {
        YIELD();
    }

    bp_queue_free(queue->work_items);
    SEM_DESTROY(&queue->cond);
    SEM_DESTROY(&queue->lock);
    lib_free(queue);
}

#ifndef WIN32
STATIC void* work_thread(void* data)
#else
STATIC DWORD WINAPI work_thread(void* data)
#endif
{
    WORKQUEUE* queue = (WORKQUEUE*) data;
    WORKITEM* work;

    while (TRUE) {

        SEM_WAIT(&queue->lock);
        ++queue->thread_available;
        SEM_POST(&queue->lock);

        SEM_WAIT(&queue->cond);

        SEM_WAIT(&queue->lock);
        --queue->thread_available;
        if (queue->state == STATE_EXITING) {
            --queue->thread_count;
            SEM_POST(&queue->lock);
#ifndef WIN32
            return NULL;
#else
            return 0;
#endif
        }

        work = (WORKITEM*) bp_queue_get(queue->work_items);
        SEM_POST(&queue->lock);
        ASSERT(work != NULL);

        work->func(work->data);

        lib_free(work);
    }

#ifndef WIN32
    return NULL;
#else
    return 0;
#endif
}

STATIC int workqueue_add_thread(WORKQUEUE* queue)
{
#ifndef WIN32
    THREAD_T tid;

    if (THR_CREATE(&tid, NULL, work_thread, queue) != 0) {
        return -1;
    }
#else
    DWORD tid;
    HANDLE thread = CreateThread(NULL, 0, work_thread, queue, 0, &tid);
#endif

    ++queue->thread_count;

    return 0;
}

int workqueue_add_item(WORKQUEUE* queue, void (*func)(void*), void* data)
{
    int rc;
    WORKITEM* work;

    SEM_WAIT(&queue->lock);

    if (queue->state != STATE_INIT) {
        SEM_POST(&queue->lock);
        return -3;
    }

    if (queue->work_items->entries == queue->work_items->max_entries) {
        bp_queue_grow(queue->work_items, QUEUE_GROW_AMOUNT);
    }

    if (queue->thread_available == 0) {
        if (workqueue_add_thread(queue) != 0 && queue->thread_count == 0) {
            SEM_POST(&queue->lock);
            return -2;
        }
    }

    work = lib_malloc(sizeof(WORKITEM));
    if (work == NULL) {
        return -1;
    }

    work->func = func;
    work->data = data;

    rc = bp_queue_put(queue->work_items, work);
    SEM_POST(&queue->lock);
    if (rc == 0) {
        SEM_POST(&queue->cond);
    }

    return rc;
}

#ifdef	UNPROVEN
extern int errno;

static
void sem_perror (char *f1, char *w, char *file, int lineno, char *f2) {
    int e = errno;

    fprintf (stderr, "%s(%s) at %s:%d ", f1, w, file, lineno);
    errno = e;
    perror (f2);
}

static
void sem_trace (char *f1, char *what, char *file, int lineno) {
    char       **p;  
    static char *w[] = {
	NULL,
    };
    
    for (p = w; *p; p++)
        if (strstr (what, *p)) {
	    fprintf (stderr, "%04x %s(%s) at %s:%d\n",
		     (unsigned int) ((unsigned long) (pthread_self ()) & 0xffff),
		     f1, what, file, lineno);
	    return;
	}
}


int	sem_init (sem_t *s, int c, char *what, char *file, int lineno) {
    sem_trace ("sem_init", what, file, lineno);

    memset (s, 0, sizeof *s);

    if ((errno = pthread_mutex_init (&s -> sem_mutex, NULL)) > 0) {
        sem_perror ("sem_init", what, file, lineno, "pthread_mutex_init");
	return 0;
    }
    if ((errno = pthread_cond_init (&s -> sem_cond, NULL)) > 0) {
	sem_perror ("sem_init", what, file, lineno, "pthread_cond_init");
	return 0;
    }
    s -> sem_count =  c;

    return 1;
}

int	sem_wait (sem_t *s, char *what, char *file, int lineno) {
    sem_trace ("sem_wait", what, file, lineno);

    if ((errno = pthread_mutex_lock (&s -> sem_mutex)) > 0) {
	sem_perror ("sem_wait", what, file, lineno, "pthread_mutex_lock");
	return 0;
    }

    while (s -> sem_count <= 0)
	if ((errno = pthread_cond_wait (&s -> sem_cond, &s -> sem_mutex)) > 0) {
	    sem_perror ("sem_wait", what, file, lineno, "pthread_cond_wait");
	    pthread_mutex_unlock (&s -> sem_mutex);
	    return 0;
	}
    
    s -> sem_count--;

    if ((errno = pthread_mutex_unlock (&s -> sem_mutex)) > 0)
	sem_perror ("sem_wait", what, file, lineno, "pthread_mutex_unlock");

    return 1;
}

int	sem_trywait (sem_t *s, char *what, char *file, int lineno) {
    int	    c;

    sem_trace ("sem_trywait", what, file, lineno);

    if ((errno = pthread_mutex_lock (&s -> sem_mutex)) > 0) {
	sem_perror ("sem_trywait", what, file, lineno, "pthread_mutex_lock");
	return 0;
    }

    if ((c = s -> sem_count) > 0)
	s -> sem_count--;

    if ((errno = pthread_mutex_unlock (&s -> sem_mutex)) > 0)
	sem_perror ("sem_trywait", what, file, lineno, "pthread_mutex_unlock");

    return ((c > 0) ? 1 : 0);
}

int	sem_post (sem_t *s, char *what, char *file, int lineno) {
    sem_trace ("sem_post", what, file, lineno);

    if ((errno = pthread_mutex_lock (&s -> sem_mutex)) > 0) {
	sem_perror ("sem_wait", what, file, lineno, "pthread_mutex_lock");
	return 0;
    }

    s -> sem_count++;

    if ((errno = pthread_cond_signal (&s -> sem_cond)) > 0)
	sem_perror ("sem_post", what, file, lineno, "pthread_cond_signal");

    if ((errno = pthread_mutex_unlock (&s -> sem_mutex)) > 0)
	sem_perror ("sem_post", what, file, lineno, "pthread_mutex_unlock");

    return 1;
}

int	sem_destroy (sem_t *s, char *what, char *file, int lineno) {
    sem_trace ("sem_destroy", what, file, lineno);

    if ((errno = pthread_cond_destroy (&s -> sem_cond)) > 0) {
	sem_perror ("sem_destroy", what, file, lineno, "pthread_cond_destroy");
	return 0;
    }
  
    return 1;
}
#endif
