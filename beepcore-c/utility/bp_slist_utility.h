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
 * $Id: bp_slist_utility.h,v 1.2 2001/10/30 06:00:40 brucem Exp $
 *
 * IW_slist_utility.h
 *
 * Generic list implemtation header file.  slist is a singly linked list.
 */
#ifndef BP_SLIST_H
#define BP_SLIST_H 1

#if defined(__cplusplus)
extern "C" 
{
#endif

typedef struct _bp_slist bp_slist;

struct _bp_slist
{
  void *data;
  bp_slist *next;
};


/* Singly linked lists */

extern bp_slist *bp_slist__new    (void);
extern void	bp_slist__free    (bp_slist *list);
extern void     bp_slist__freeall (bp_slist *list);
extern bp_slist *bp_slist__append (bp_slist *list,void *data);
extern bp_slist *bp_slist__prepend(bp_slist *list,void *data);
extern bp_slist *bp_slist__insert (bp_slist *list,void *data,int index);
extern bp_slist *bp_slist__add    (bp_slist *list1,bp_slist *list2);
extern bp_slist *bp_slist__remove (bp_slist *list,void *data);
extern bp_slist *bp_slist__remove_link(bp_slist *list,bp_slist *rmv);
extern bp_slist *bp_slist__remove_index(bp_slist *,int i);
extern bp_slist *bp_slist__copy   (bp_slist *list);
extern bp_slist *bp_slist__index  (bp_slist *list,int index);
extern bp_slist *bp_slist__find   (bp_slist *list,void *data);
extern int bp_slist__findindex    (bp_slist *list,void *data);
extern bp_slist *bp_slist__last   (bp_slist *list);
extern int bp_slist__length       (bp_slist *list);
extern void *bp_slist__index_data (bp_slist *list,int index);
extern char *bp_slist__lAlloc(bp_slist **funclist,int isize);
extern char *bp_slist__lstrdup(bp_slist **funclist,char *str);
extern bp_slist *bp_slist__join(bp_slist *list1,bp_slist *list2);
extern char *bp_slist__lstrcat(bp_slist **funclist,char *oldstr,char *str);
extern void bp_gen__Free(char **p);
extern int bp_gen__strlen(const char *str);
extern bp_slist *bp_slist__split(char *pStringtoSplit,char cSplitChars);

#if defined(__cplusplus)
}
#endif

#endif


