/*
 * Copyright (c) 2002 William J. Mills.  All rights reserved.
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
 * $Id: bp_clist.h,v 1.4 2002/03/29 03:11:43 sweetums Exp $
 *
 * IW_clist.h
 *
 * This is a circular list implementation, originally intended for use in
 * a circular scheduling queue.  It specifically minimizes memory 
 * allocation and free.   Memory once alocated in the list is not freed until
 * the list is destroyed.
 */
#ifndef BP_CLIST_H
#define BP_CLIST_H 1

#if defined(__cplusplus)
extern "C" 
{
#endif

struct bp_clist;

/* Circlular lists */

/**
 *
 *
 * @param 
 *
 * @return
 **/
extern struct bp_clist *bp_clist_new(void);
/**
 * Destroy a clist and free all memory allocated for it.
 *
 * @param list The clist to operate on.
 *
 * @return A pointer to the new clist, NULL on any failure.
 **/
extern void bp_clist_free(struct bp_clist *list);
/**
 * Add an item to the clist.  This will be added just after the current item 
 * in the list.
 *
 * @param list The clist to operate on.
 **/
extern int bp_clist_add(struct bp_clist *list, void *data);
/**
 * Deletes the current item from the clist and puts the freed node on the free list.
 *
 * @param list The clist to operate on.
 *
 * @return 1 for sucess, 0 otherwise.
 **/
extern int bp_clist_delete(struct bp_clist *list);
/**
 * Scans through the list and removes the first occurrence of the data pointer.
 *
 * @param list The clist to operate on.
 *
 * @param data The content pointer to be found and removed.
 *
 * @return 1 for sucess, 0 otherwise.
 **/
extern int bp_clist_remove(struct bp_clist *list, void *data);
/**
 * Find the node containing the data pointer and make it the current 
 * node.
 *
 * @param list The clist to operate on.
 *
 * @return 1 for sucess, 0 otherwise.
 **/
extern int bp_clist_find(struct bp_clist *list, void *data);
/**
 * Return the current node's data pointer.
 *
 * @param list The clist to operate on.
 *
 * @return 1 for sucess, 0 otherwise.
 **/
extern void *bp_clist_this(struct bp_clist *list);
/**
 * Increment the current node to the next item in the list.
 *
 * @param list The clist to operate on.
 **/
extern void *bp_clist_next(struct bp_clist *list);
/**
 * Return the number of items in the list.
 *
 * @param list The clist to operate on.
 **/
extern int bp_clist_length (struct bp_clist *list);


#if defined(__cplusplus)
}
#endif

#endif


