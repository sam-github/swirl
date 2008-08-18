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
/***-------------------------------------------------------------------------*/
/***                                                                         */
/*** @(#) $Id: bp_slist_utility.c,v 1.2 2001/10/30 06:00:40 brucem Exp $                                   */
/***                                                                         */
/***                                                                         */
/*** Name:          IW_slist.c                                               */
/***                                                                         */
/***                                                                         */
/*** Description:   Single linked list                                       */
/***    A node of these single linked list is defined as                     */
/***           struct _IW_slist                                              */
/***               {                                                         */
/***               void *data;                                               */
/***               IW_slist *next;                                           */
/***            } _IW_slist;                                                 */
/***                                                                         */
/***        The data portion of the struct has a type of (void *) so any     */ 
/***        defined data type can be cast or represented.                    */
/***        It is the responsibility of the programmers to allocate and free */
/***        the memory associated with the data element.                     */
/***                                                                         */
/*** History:                                                                */
/***                                                                         */
/***                                                                         */
/***-------------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "bp_slist_utility.h"


/*---------------------------------------------------------------**
**                                                               **
**  Name: IW_slist__new                                           **
**                                                               **
**  Description:                                                 **
**  Allocates memory for single link list node                   **
**      used by IW_slist__append, IW_slist__prepend,               **
**      and IW_slist__insert,IW_slist__copy                        **
**      Most applications should not call this function directly **
**---------------------------------------------------------------*/
bp_slist* bp_slist__new(void)
{
    bp_slist *list;

    list = (bp_slist *)malloc((int)sizeof(bp_slist));
    list->data = NULL;
    list->next = NULL;
    return list;
}



/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__free                                         **
**                                                               **
**  Description:                                                 **
**  frees memory for single link list node                       **
**      used by bp_slist__remove, bp_slist__remove_link,         **
**      and bp_slist__remove_index                               **
**      Most application should not call this function directly  **
**---------------------------------------------------------------*/

void bp_slist__free (bp_slist *list)
{
    bp_slist *templist;
    bp_slist *prevlist;

    if (!list)
        return;

    templist = list->next;
    prevlist = list;
    while (prevlist)
        {
        bp_gen__Free((char **)&prevlist);
        prevlist = templist;
        if (prevlist)
           templist = prevlist->next;
        }
}

/* 
   frees list->data for complete list then frees the list itself
*/

void bp_slist__freeall(bp_slist *list)
{
    bp_slist *templist;
    bp_slist *prevlist;

    if (!list)
        return;
    templist = list->next;
    prevlist = list;
    while (prevlist)
        {
        bp_gen__Free((void *)&prevlist->data);
        bp_gen__Free((char **)&prevlist);
        prevlist = templist;
        if (prevlist)
           templist = prevlist->next;
        }
}


/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__append                                        **
**                                                               **
**  Description:                                                 **
**  Adds new node to end of single list                          **
**---------------------------------------------------------------*/

bp_slist *bp_slist__append (bp_slist *list,void *data)
{
    bp_slist *newlist;
    bp_slist *last;

    newlist = bp_slist__new();
    newlist->data = data;

    if (list)
      {
      last = bp_slist__last (list);
      last->next = newlist;
      return list;
      }
    return newlist;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__prepend                                       **
**                                                               **
**  Description:                                                 **
**  Adds new node to beginning of single list                    **
**---------------------------------------------------------------*/

bp_slist *bp_slist__prepend (bp_slist *list,void *data)
{
    bp_slist *newlist;

    newlist = bp_slist__new();
    newlist->data = data;
    newlist->next = list;

    return newlist;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__insert                                        **
**                                                               **
**  Description:                                                 **
**  Inserts a new node at 'index' position                       **
**      if index is 0 new node is prepended,                     **
**      if index is less than 0 new node is appended             **
**---------------------------------------------------------------*/


bp_slist *bp_slist__insert (bp_slist *list,void *data,int index)
{
    bp_slist *prevlist_list;
    bp_slist *templist_list;
    bp_slist *newlist;

    if (index < 0)
        return bp_slist__append (list, data);
    if (index == 0)
        return bp_slist__prepend (list, data);
    newlist = bp_slist__new();
    newlist->data = data;
    if (list == NULL)
        return newlist;
    prevlist_list = NULL;
    templist_list = list;

    while ((index > 0) && templist_list)
        {
        prevlist_list = templist_list;
        templist_list = templist_list->next;
        index--;
        }

    if (prevlist_list)
        {
        newlist->next = prevlist_list->next;
        prevlist_list->next = newlist;
        }
    else
        {
        newlist->next = list;
        list = newlist;
        }

    return list;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__join                                          **
**                                                               **
**  Description:                                                 **
**  Single list2 is appended to single list1                     **
**---------------------------------------------------------------*/



bp_slist *bp_slist__join(bp_slist *list1, bp_slist *list2)
{
    bp_slist *templist;

    if (list2)
        {
        if (list1)
           {
           templist = bp_slist__last(list1);
           templist->next = list2;
           }
        else
	   list1 = list2;
        }

    return list1;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__remove                                        **
**                                                               **
**  Description:                                                 **
**  removes and frees a node based on memory location of data    **
**---------------------------------------------------------------*/

bp_slist *bp_slist__remove (bp_slist *list,void *data)
{
    bp_slist *templist;
    bp_slist *prevlist;

    prevlist = NULL;
    templist = list;

    while (templist)
        {
        if (templist->data == data)
	    {
	    if (prevlist)
	        prevlist->next = templist->next;
	    if (list == templist)
	        list = list->next;

	    templist->next = NULL;
	    bp_slist__free (templist);

	    break;
	    }
        prevlist = templist;
        templist = templist->next;
        }
    return list;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__remove_link                                   **
**                                                               **
**  Description:                                                 **
**  removes and frees node based on memory location of node      **
**---------------------------------------------------------------*/

bp_slist *bp_slist__remove_link (bp_slist *list,bp_slist *rmv)
{
    bp_slist *templist;
    bp_slist *prevlist;

    prevlist = NULL;
    templist = list;

    while (templist)
        {
        if (templist == rmv)
            {
	    if (prevlist)
	        prevlist->next = templist->next;
	    if (list == templist)
	        list = list->next;

	    templist->next = NULL;
            bp_slist__free(templist);
	    break;
	    }

        prevlist = templist;
        templist = templist->next;
        }

    return list;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__remove_index                                    **
**                                                               **
**  Description:                                                 **
**  remove and frees a node based on index position in list      **
**---------------------------------------------------------------*/

bp_slist *bp_slist__remove_index(bp_slist *list,int index)
{
    bp_slist *templist;
    bp_slist *prevlist;

    prevlist = NULL;
    templist = list;
    while (index > 0 && templist)
        {
        prevlist = templist;
        templist = templist->next;
        index--;
        }
    if (templist)
        {
        if (prevlist)
            prevlist->next = templist->next;
        if (list == templist)
            list = list->next;
        templist->next = NULL;
        bp_slist__free(templist);
        }
    return list;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__copy                                          **
**                                                               **
**  Description:                                                 **
**  Returns a copy of the single list                            **
**---------------------------------------------------------------*/

bp_slist *bp_slist__copy (bp_slist *list)
{
    bp_slist *newlist = NULL;
    bp_slist *last;

    if (list)
        {
        newlist = bp_slist__new();
        newlist->data = list->data;
        last = newlist;
        list = list->next;
        while (list)
	    {
	    last->next = bp_slist__new();
	    last = last->next;
	    last->data = list->data;
	    list = list->next;
	    }
        }
    return newlist;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__index                                           **
**                                                               **
**  Description:                                                 **
**    Return the list(node) by index position, 0 is first node   **
**---------------------------------------------------------------*/

bp_slist *bp_slist__index(bp_slist *list,int index)
{
   while (index > 0 && list)
       {
       index--;
       list = list->next;
       }
    return list;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__index_data                                      **
**                                                               **
**  Description:                                                 **
**  Return the data associated with the link(node) by index      **
**  position                                                     **
**---------------------------------------------------------------*/

void *bp_slist__index_data (bp_slist *list,int index)
{
    while (index > 0 && list)
        {
        index--;
        list = list->next;
        }
    if (list)
        return list->data;
    return NULL;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__find                                          **
**                                                               **
**  Description:                                                 **
**  Return the list(node) that matchs the memory location of data **
**---------------------------------------------------------------*/

bp_slist *bp_slist__find (bp_slist *list,void *data)
{
    while (list)
        {
        if (list->data == data)
            break;
        list = list->next;
        }
    return list;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__findindex                                    **
**                                                               **
**  Description:                                                 **
**  Return the index position of the list(node) that matches the **
**      memory location of data                                  **
**---------------------------------------------------------------*/

int bp_slist__findindex (bp_slist *list,void *data)
{
    int i=0;

    while (list)
        {
        if (list->data == data)
	    return i;
        i++;
        list = list->next;
        }
    return (-1);
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__last                                          **
**                                                               **
**  Description:                                                 **
**  Return the last node in the list                             **
**---------------------------------------------------------------*/

bp_slist *bp_slist__last (bp_slist *list)
{
    if (list)
        {
        while (list->next)
	    list = list->next;
        }
    return list;
}

/*---------------------------------------------------------------**
**                                                               **
**  Name: bp_slist__length                                       **
**                                                               **
**  Description:                                                 **
**  Return the number of nodes in the list                       **
**---------------------------------------------------------------*/

int bp_slist__length (bp_slist *list)
{
    int length=0;

    while (list)
        {
        length++;
        list = list->next;
        }
    return length;
}

/*

   allocate a char * of isize and add to list
*/

char *bp_slist__lAlloc(bp_slist **funclist,int isize)
{
char *tmp;

tmp = (char *)malloc(isize);
*funclist = bp_slist__append(*funclist,tmp);
return tmp;
}

/* 
   strdup a char * and and to memory list
*/

char *bp_slist__lstrdup(bp_slist **funclist,char *str)
{
int iLen;
char *tmp;

iLen = bp_gen__strlen(str);
iLen++;
tmp = bp_slist__lAlloc(funclist,iLen);
if (iLen > 1)
    strcpy(tmp,str);
else
    tmp[0] = '\0';
return tmp;
}


/*

  strcat a char * to existing list->data char * and
  reallocate memory
*/
char *bp_slist__lstrcat(bp_slist **funclist,char *oldstr,char *str)
{
bp_slist *tlist=NULL;
char *tmp;
int i;

tlist = bp_slist__find(*funclist,oldstr);
if (tlist && str)
    {
    i=bp_gen__strlen(oldstr)+bp_gen__strlen(str)+1;
    tmp = (char *)malloc(i);
    sprintf(tmp,"%s%s",oldstr,str);
    bp_gen__Free(&oldstr);
    tlist->data = tmp;
    return tmp;
    }
return oldstr;
}

/*
   free memory and set to NULL
*/

void bp_gen__Free(char **p)
{
    if (!p)
        return;
    if (!*p)
        return;
    free((char *)*p);
    *p=NULL;
    return;
}

/* 
  return strlen of string testinf for NULL
*/

int bp_gen__strlen(const char *str)
{
    if (!str)
        return 0;
    return (strlen(str));
}



bp_slist *bp_slist__split(char *pStringtoSplit,char cSplitChars)
{
    bp_slist *splitlist=NULL;
    char *pItem;
    int i,iLen,j,iMalLen;

    iLen = strlen(pStringtoSplit);
    if (iLen == 0)
        {
        splitlist = bp_slist__append(splitlist,NULL);
        return splitlist;
        }
    j=0;
    for (i=0;i<iLen;i++)
        {
        if (pStringtoSplit[i] == cSplitChars)
            {
            iMalLen = i-j;
            if (iMalLen == 0)
                {
                splitlist = bp_slist__append(splitlist,NULL);
                j=i+1;
                }
            else
                {
                pItem = (char *)malloc(iMalLen+1);
                memcpy((char *)pItem,(char *)&pStringtoSplit[j],iMalLen);
                pItem[iMalLen]='\0';
                splitlist = bp_slist__append(splitlist,pItem);
                j=i+1;
                }
            }
        }
    if (i != j)
        {
        pItem = (char *)malloc(iLen-j+1);
        memcpy((char *)pItem,(char *)&pStringtoSplit[j],iLen-j);
                pItem[iLen - j]='\0';
        splitlist = bp_slist__append(splitlist,pItem);
        }
    return splitlist;
}
       
