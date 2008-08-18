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
 * $Id: bp_queue.h,v 1.4 2002/03/26 23:56:20 huston Exp $
 *
 * IW_queue.h
 *
 * Provides a queue utility for managing a queue struct.
 */

#ifndef BP_QUEUE_H
#define BP_QUEUE_H 1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _bp_queue_node bp_queue_node;

struct _bp_queue_node
{
    int used;
    int dataduped;
    void *data;
    bp_queue_node *next;
    bp_queue_node *prev;
};



typedef struct
{
    int entries;
    int max_entries;
    bp_queue_node *top;
    bp_queue_node *bottom;
} bp_queue;
/*
 * bp_queue_node_new
 * bp_queue_node_free
 *
 * constructor and destructor for nodes.
 *
 * bp_queue_node_new returns the pointer to the new memory, or NULL
 * on error.
 */
extern bp_queue_node *bp_queue_node_new(void);
extern void bp_queue_node_free(bp_queue_node *q);
/*
 * bp_queue_new
 * bp_queue_free
 *
 * Constructor and destructor for queues.
 *
 * bp_queue_new returns the pointer to the new memory, or NULL
 * on error.
 */
extern bp_queue *bp_queue_new(int size);
extern int bp_queue_free(bp_queue *q);
/*
 * bp_queue_put
 * bp_queue_put_dup
 *
 * Queue a new item at the tail of the queue.  For bp_queue_put the caller
 * must manage the memory for the stored pointer and free it after
 * de-queueing.  bp_queue_put_dup manages the memory for the duplicated 
 * item.
 */
extern int bp_queue_put_dup(bp_queue *q,void *data,int size);
extern int bp_queue_put(bp_queue *q,void *data);
extern int bp_queue_put_and_grow(bp_queue *q, void *data, int size);

/*
 * bp_queue_peek
 * bp_queue_get
 *
 * Return the first element in the queue.  bp_queue_get dequeues the
 * first entry and returns the pointer to the contents, bp_queue_peek 
 * does not dequeue the first entry.
 *
 * The caller must manage memory (alloc/free) for items queued with 
 *  bp_queue_put, and should NOT free items queued with bp_queue_put_dup.
 */
extern void *bp_queue_peek(bp_queue *q);
extern void *bp_queue_get(bp_queue *q);
/*
 * bp_queue_grow
 *
 * Adds the size number of elements to the queue.
 *
 * Returns:  0 on success
 *          -1 on memory alloc error. 
 */
extern int bp_queue_grow(bp_queue *q,int size);

extern int bp_queue_clear(bp_queue *q);
/*
 * bp_queue_len
 *
 * Return the number of items queued.
 */
extern int bp_queue_len(bp_queue *q);

#ifdef __cplusplus
}
#endif

#endif
