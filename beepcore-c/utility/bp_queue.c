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
 * $Id: bp_queue.c,v 1.5 2002/03/26 23:56:20 huston Exp $
 *
 * IW_queue.c
 *
 * Implementation of the queue utility functions
 */
#ifndef WIN32
#include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "bp_queue.h"

bp_queue_node *bp_queue_node_new(void)
{
    bp_queue_node *queue;

    if (NULL != (queue = (bp_queue_node *)malloc((int)sizeof(bp_queue_node)))) 
      {
	queue->used = 0;
	queue->dataduped = 0;
	queue->data = NULL;
	queue->next = NULL;
	queue->prev = NULL;
      }
    return queue;
}


void bp_queue_node_free(bp_queue_node *q)
{

    if (!q)
        return;

    if (q->dataduped)
        free(q->data);
    free((bp_queue_node *)q);    
}


bp_queue *bp_queue_new(int size)
{
    int i;
    bp_queue_node *tmp,*qn;
    bp_queue *q;

    if (size <= 1)
        return NULL;
    if (NULL != (q = (bp_queue *)malloc((int)sizeof(bp_queue)))) 
      {
	q->entries = 0;
	q->max_entries = 1;
	q->top = bp_queue_node_new();
	q->bottom = q->top;
	qn = q->bottom;
      
	for (i=0;i<(size-1);i++)
	  {
	    if (NULL == (tmp=bp_queue_node_new()))
	      {
		bp_queue_free(q);
		return(NULL);
	      }
	    q->max_entries++;
	    qn->next = tmp;
	    tmp->prev= qn;
	    qn=tmp;
	  }
      
	qn->next = q->bottom;
	q->top->prev = qn;
      }

    return q;
}


int bp_queue_free(bp_queue *q)
{
    int i;
    bp_queue_node *qn;

    if (!q)
        return 0;
    qn = q->top;
    for (i=0;i<q->max_entries;i++)
    {
        q->top = qn->next;
        bp_queue_node_free(qn);
        qn = q->top;
    }
    free((bp_queue *)q);
    return 0;
}

int bp_queue_clear(bp_queue *q)
{
    int i;
    bp_queue_node *qn;

    if (!q)
        return 0;
    qn = q->top;
    for (i=0;i<q->max_entries;i++)
    {
        q->top = qn->next;
        qn->used = 0;
        if (qn->dataduped)
            free(qn->data);
        qn->dataduped=0;
        qn->data=NULL;
        qn = q->top;
    }
    q->entries=0;
    return 0;
}    

int bp_queue_put_dup(bp_queue *q,void *data,int size)
{
    bp_queue_node *qn;

    qn = q->bottom->next;
    if (qn->used)
    {
        return 1;
    }
    q->bottom=qn;
    qn->used=1;
    if (qn->dataduped)
        free(qn->data);
    qn->dataduped=1;
    qn->data = malloc(size);
    memcpy(qn->data,data,size);
    if (!q->entries)
        q->top = q->bottom;
    q->entries++;
    qn->used=1;
    return 0;
}



int bp_queue_put(bp_queue *q,void *data)
{
    bp_queue_node *qn;

    qn = q->bottom->next;
    if (qn->used)
    {
        return 1;
    }
    q->bottom=qn;
    qn->used=1;
    if (qn->dataduped)
        free(qn->data);
    qn->dataduped=0;
    qn->data = data;
    if (!q->entries)
        q->top = q->bottom;
    q->entries++;
    qn->used=1;
    return 0;
}


int bp_queue_put_and_grow(bp_queue *q, void *data, int size)
{
    int err;

    err = bp_queue_put(q, data);
    if (err == 0) {
        return 0;
    }

    err = bp_queue_grow(q, size);
    if (err != 0) {
        return err;
    }

    return bp_queue_put(q, data);
}


void *bp_queue_peek(bp_queue *q)
{
    bp_queue_node *qn;

    qn = q->top;
    if (!qn->used)
        return NULL;
    return qn->data;
}


void *bp_queue_get(bp_queue *q)
{

    bp_queue_node *qn;
    void *ret;

    if (!q)
        return NULL;
    qn = q->top;
    if (!qn->used && !q->entries)
        return NULL;
    ret = qn->data;
    qn->used=0;
    q->top = qn->next;
    q->entries--;
    return ret;
}


int bp_queue_grow(bp_queue *q,int size)
{
    bp_queue_node *qn,*tmp,*btmp;
    int i;

    qn = q->top;

    if (q->top != q->bottom->next)
        return 1;
    if (!size)
        return 2;
    /* find new bottom */


    btmp = q->bottom->next;
    q->bottom = q->top->prev;
    qn = q->bottom;

    for (i=0;i<size;i++)
      {
	if (NULL == (tmp=bp_queue_node_new())) 
	  {
	    return(-1);
	  }
        q->max_entries++;
        qn->next = tmp;
        tmp->prev= qn;
        qn=tmp;
      }
    
    qn->next = btmp;
    q->top->prev = qn;
    return 0;
}

extern int bp_queue_len(bp_queue *q)
{
    if (!q)
        return 0;
    else
        return q->entries;
}

	
