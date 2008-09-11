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
 * $Id: bp_clist.c,v 1.3 2002/03/29 03:11:42 sweetums Exp $
 *
 * Implementation of the circluar list widget.
 */

#include <string.h>
#include "bp_malloc.h"
#include "bp_clist.h"

/*
 * Private structures.
 */
struct bp_clistitem; 

struct bp_clist {
    int length;
    int free;
    struct bp_clistitem * current;
    struct bp_clistitem * freelist;
};

struct bp_clistitem {
    struct bp_clistitem * next;
    void * data;
};

/*
 *
 */
extern struct bp_clist* bp_clist_new(void) {
    struct bp_clist* new;

    if ((new = lib_malloc(sizeof(struct bp_clist)))) {
	memset(new, 0, sizeof(struct bp_clist));
    }
    return new;
}
/*
 *
 */
void my_clist_kill(struct bp_clistitem * list){
    struct bp_clistitem * node;
    struct bp_clistitem * tail;

    if (!list)
      return;

    node = list->next;
    tail = list;
    tail->next = NULL;
    while (node) {
	    tail = node->next;
	    lib_free(node);
	    node = tail;
    }
    
    return;
}
/*
 *
 */
extern void bp_clist_free(struct bp_clist *list){

    if (!list)
      return;

    my_clist_kill(list->current);
    my_clist_kill(list->freelist);
    lib_free(list);
}
/*
 *
 */
extern int bp_clist_add(struct bp_clist *list, void *data) {
    struct bp_clistitem * node;

    if (!list || !data) 
	return 0;

    /* get our new node */ 
    if (list->freelist) {
	node = list->freelist;
	list->freelist = list->freelist->next;
	list->free--;
    } else {
	if (NULL == (node = lib_malloc(sizeof(struct bp_clistitem)))) {
	    return 0;
	}
    }

    /* now add to the list */
    node->data = data;
    
    if (NULL == list->current) {
	    node->next = node;
    } else {
	node->next = list->current->next;
	list->current->next = node;
    }

    /* chage current to our new node */
    list->current = node;
    list->length++;

    return 1;
}
/*
 * Only odd bit here is that we actually need to preserve the current node
 * since someone points to it, so we moce the contents of next into this and
 * remove next.
 */
extern int bp_clist_delete(struct bp_clist *list){
    struct bp_clistitem * node;    

    if (!list)
	return 0;
    if (!list->current)
	return 0;

    if (list->current == list->current->next) {
	node = list->current;
	list->current = NULL;
    } else {
	node = list->current->next;
	list->current->data = node->data;
	list->current->next = node->next;
    }
    list->length--;

    node->next = list->freelist;
    node->data = NULL;
    list->freelist = node;
    list->free++;
  
    return 1;
}
/*
 *
 */
extern int bp_clist_remove(struct bp_clist *list, void *data){
    if (!list || !data) 
	return 0;

    if (bp_clist_find(list, data)) {
	return bp_clist_delete(list);
    }

    return 0;
}
/*
 *
 */
extern int bp_clist_find(struct bp_clist *list, void *data){
    int i;

    if (!list)
	return 0;

    for (i = list->length; i; i--) {
	if (list->current->data == data)
	    return 1;
	list->current = list->current->next;
    }

    return 0;
}
/*
 *
 */
extern void *bp_clist_this(struct bp_clist *list){
    if (list) {
	if (list->current) {
	    return list->current->data;
	}
    }
    return NULL;
}
/*
 *
 */
extern void *bp_clist_next(struct bp_clist *list){
    if (list) {
	if (list->current) {
	    list->current = list->current->next;
	    return list->current->data;
	}
    }
    return NULL;    
}
/*
 *
 */
extern int bp_clist_length (struct bp_clist *list){
    if (list)
	return list->length;
    else 
	return 0;
}

