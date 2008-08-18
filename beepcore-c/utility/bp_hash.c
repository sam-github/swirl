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
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bp_hash.h"
#include "bp_slist_utility.h"

/* private prototypes */
char *bp_hash_entry(BP_HASH_TABLE *htable,void *key,int keysize,void *value,int valuesize,int vdup);
unsigned long bp_hash_reckon_nbr(void *key,int ilen);
HASH_PAGE *bp_hash_make_page(BP_HASH_TABLE *hashtable,int thispage);
int bp_hash_cmpkeys(BP_HASH_TABLE *htable,void *key1,void *key2);
unsigned long bp_hash_nbr_to_table_size(BP_HASH_TABLE *htable,void *key,int len);
char *bp_hash_strdup(char *str);
void *bp_hash_search_nextkey(BP_HASH_SEARCH *hs);
void *bp_hash_search_firstkey(BP_HASH_SEARCH *hs);
int bp_hash_entry_delete(BP_HASH_TABLE *htable,void *key);
void *bp_hash_get_value(BP_HASH_TABLE *htable,void *key,int size);
void **bp_hash_get_key(BP_HASH_TABLE *htable,void *key,int size);

/* 
   create a new hash table 

   parameters:
              entries: the number of entries desired.
              type:    either BP_HASH_STRING for string keys 
                       or BP_HASH_INT for interger keys
   returns a hash table pointer
*/
   

BP_HASH_TABLE *bp_hash_table_init(int entries,int type)
{
    
    BP_HASH_TABLE *retHash=NULL;
    unsigned int i;
    HASH_PAGE *hpage;

    if (type != BP_HASH_STRING && type != BP_HASH_INT && type != BP_HASH_OTHER)
        return NULL;
    retHash = (BP_HASH_TABLE *)malloc(sizeof(BP_HASH_TABLE));
    if (!retHash)
        return NULL;
    retHash->lmem = NULL;
    retHash->datapages = NULL;
    retHash->nbr_pages = (entries/HASH_PAGE_SIZE)+1;
    retHash->max_entries = retHash->nbr_pages*HASH_PAGE_SIZE;
    retHash->nbr_entries = 0;
    retHash->hash_type = type;
    for (i=0;i<retHash->nbr_pages;i++)
    {
        hpage = bp_hash_make_page(retHash,(i+1));
        if (!hpage)
        {
            bp_slist__free(retHash->datapages);
            retHash->datapages=NULL;
            bp_hash_free((void **)&hpage);
            return NULL;
        }
        retHash->datapages = bp_slist__append(retHash->datapages,hpage);
        retHash->lmem = bp_slist__prepend(retHash->lmem,hpage);
    }
    return retHash;
}



HASH_PAGE *bp_hash_make_page(BP_HASH_TABLE *hashtable,int thispage)
{
    HASH_PAGE *hpage=NULL;
    int i;

    hpage = (HASH_PAGE *)malloc(sizeof(HASH_PAGE));
    if (!hpage)
        return NULL;
    hpage->this_hash_page = thispage;
    for (i=0;i<HASH_PAGE_SIZE;i++)
    {
        hpage->hash_nbr[i]   = NO_KEY;
        hpage->data[i].key   = NULL;
        hpage->data[i].value = NULL;
        hpage->data[1].vdup  = 0;
        hpage->data[i].ks    = 0;
        hpage->data[i].vs    = 0;
        hpage->data[i].collision = NULL;
    }
    return hpage;
}

int bp_hash_table_fin(BP_HASH_TABLE **hashtable)
{
    bp_slist *datapage=NULL;
    bp_slist *tl,*pl;
    HASH_PAGE *hpage;
    BP_HASH_TABLE *ht;
    unsigned int i;
    int j;

    ht = *hashtable;
    for (i=0;i<ht->nbr_pages;i++)
    {
        datapage = bp_slist__index(ht->datapages,i);
        hpage = (HASH_PAGE *)datapage->data;
        for (j=0;j<HASH_PAGE_SIZE;j++)
        {
            bp_slist__free(hpage->data[j].collision);
            hpage->data[j].collision=NULL;
        }
    }
    bp_slist__free(ht->datapages);
    ht->datapages=NULL;
    pl = ht->lmem;
    tl = pl->next;
    while (pl)
    {
        bp_hash_free(&pl->data);
        pl = tl;
        if (pl)
            tl=pl->next;
    }
    bp_slist__free(ht->lmem);
    ht->lmem=NULL;
    bp_hash_free((void **)hashtable);
    return 0;
}

/*

   add or update the key/value pair in the hash table
   if the key is not found they are added.
   if the key is found the value is replaced.

   A return of NULL is succesful otherwise an error message is returned.
   It is the programmers responsibility to free the error message.

*/

char *bp_hash_entry(BP_HASH_TABLE *htable,void *key,int keysize,void *value,int valuesize,int vdup)
{
    unsigned long hash_nbr,index;
    unsigned long *deref;
    int len,page_nbr,j,i,k;
    bp_slist *current_page,*tmplist;
    HASH_PAGE *hpage;
    HASH_KEY_VALUE *newdata;
    void *newkey,*newvalue;
    char *errmsg;

/*    if (htable->nbr_entries == htable->max_entries)
    {
        errmsg = (char *)bp_hash_strdup("Hash table is full");
        return errmsg;
    } */
    len = keysize;
    if (htable->hash_type)
    {
        deref = (unsigned long *)key;
        if (*deref == NO_KEY)
        {
            errmsg = bp_hash_strdup("Reserved integer key word");
            return errmsg;
        }
    }
    else
    {
        if (len == 0)
        {
            errmsg = bp_hash_strdup("Hash key cannot be NULL");
            return errmsg;
        }
    }
    newkey = key;
    if (htable->hash_type != BP_HASH_OTHER)
    {
        newkey = malloc(len);
        if (!newkey)
        {
            errmsg = bp_hash_strdup("Allocation error");
            return errmsg;
        }
        memcpy(newkey,key,len);
        htable->lmem = bp_slist__prepend(htable->lmem,newkey);
    }
    newvalue = value;
    if (vdup)
    {
        newvalue = malloc(valuesize);
        memcpy(newvalue,value,valuesize);
        htable->lmem = bp_slist__prepend(htable->lmem,newvalue);
    }
    hash_nbr = bp_hash_nbr_to_table_size(htable,key,len);
    page_nbr = hash_nbr/HASH_PAGE_SIZE;
    current_page = bp_slist__index(htable->datapages,page_nbr);
    hpage = (HASH_PAGE *)current_page->data;
    index = hash_nbr-(page_nbr*HASH_PAGE_SIZE);
    if (hpage->hash_nbr[index] == NO_KEY)
    {
        /*  empty hash node - populate*/
        hpage->hash_nbr[index] = hash_nbr;
        hpage->data[index].key = newkey;
        hpage->data[index].value = newvalue;
        hpage->data[index].vdup  = vdup;
        hpage->data[index].ks    = keysize;
        hpage->data[index].vs    = valuesize;
        hpage->data[index].collision = NULL;
        htable->nbr_entries++;
    }
    else
    {
        /* duplicate hash number found */
        j = bp_hash_cmpkeys(htable,hpage->data[index].key,newkey);
        if (j == 0)
        {
            /* duplicate key found in hash node - update */
            if (hpage->data[index].vdup)
                bp_hash_free(&hpage->data[index].value);
            hpage->data[index].vdup  = vdup;
            hpage->data[index].value = newvalue;
            hpage->data[index].ks    = keysize;
            hpage->data[index].vs    = valuesize;
            if (htable->hash_type != BP_HASH_OTHER)
                bp_hash_free(&newkey);
            return NULL;
        }
        i = bp_slist__length(hpage->data[index].collision);
        if (i)
        {
            /* collision list found for this node */
            for (j=0;j<i;j++)
            {
                tmplist = bp_slist__index(hpage->data[index].collision,j);
                newdata = (HASH_KEY_VALUE *)tmplist->data;
                k = bp_hash_cmpkeys(htable,newdata->key,newkey);
                if (k == 0)
                {
                    /* duplicate key found in collision list - update value */
                    if (newdata->vdup)
                        bp_hash_free(&hpage->data[index].value);
                    newdata->vdup  = vdup;
                    newdata->value = newvalue;
                    newdata->ks    = keysize;
                    newdata->vs    = valuesize;
                    if (htable->hash_type != BP_HASH_OTHER)
                        bp_hash_free(&newkey);
                    return NULL;
                }
            }
        }
        /* duplicate hash number with no matching key -- add to collision list */
        newdata = (HASH_KEY_VALUE *)malloc(sizeof(HASH_KEY_VALUE));
        if (!newdata)
        {
            bp_hash_free(&newkey);
            errmsg = bp_hash_strdup("Allocation error");
            return errmsg;
        }
        newdata->key = newkey;
        newdata->value = newvalue;
        newdata->collision = NULL;
        newdata->vdup = vdup;
        newdata->ks    = keysize;
        newdata->vs    = valuesize;
        hpage->data[index].collision = bp_slist__append(hpage->data[index].collision,newdata);
        htable->nbr_entries++;
        htable->lmem = bp_slist__prepend(htable->lmem,newdata);
    }
    return NULL;
}


void *bp_hash_other_entry(BP_HASH_TABLE *htable,void *key,int keysize,void *value)
{
    char *errmsg;

    errmsg = bp_hash_entry(htable,key,keysize,value,0,0);
    return errmsg;
}

void *bp_hash_other_entry_dup(BP_HASH_TABLE *htable,void *key,int keysize,void *value,int size)
{
    char *errmsg;

    errmsg = bp_hash_entry(htable,key,keysize,value,size,1);
    return errmsg;
}


char *bp_hash_int_entry(BP_HASH_TABLE *htable,unsigned long key,void *value)
{
    unsigned long temp;
    void *newkey;
    char *errmsg;

    temp = key;
    newkey = &temp;
    errmsg = bp_hash_entry(htable,newkey,sizeof(unsigned long),value,0,0);
    return errmsg;
}

char *bp_hash_int_entry_dup(BP_HASH_TABLE *htable,unsigned long key,void *value,int size)
{
    unsigned long temp;
    void *newkey;
    char *errmsg;

    temp = key;
    newkey = &temp;
    errmsg = bp_hash_entry(htable,newkey,sizeof(unsigned long),value,size,1);
    return errmsg;
}

char *bp_hash_string_entry(BP_HASH_TABLE *htable,char *key,void *value)
{
    char *errmsg;
    int ks;

    ks = bp_gen__strlen(key)+1;
    errmsg = bp_hash_entry(htable,key,ks,value,0,0);
    return errmsg;
}

char *bp_hash_string_entry_dup(BP_HASH_TABLE *htable,char *key,void *value,int size)
{
    char *errmsg;
    int ks;

    ks = bp_gen__strlen(key)+1;
    errmsg = bp_hash_entry(htable,key,ks,value,size,1);
    return errmsg;
}




/*

   Get the hash value 

   parameters:
               htable: the hash table pointer
               key:    the key to search
   returns the value if found
   otherwise the return is NULL

*/
void *bp_hash_int_get_value(BP_HASH_TABLE *htable,unsigned long key)
{
    return bp_hash_get_value(htable,&key,sizeof(unsigned long));
}

void *bp_hash_string_get_value(BP_HASH_TABLE *htable,char *key)
{
    return bp_hash_get_value(htable,key,(bp_gen__strlen(key)+1));
}

void *bp_hash_other_get_value(BP_HASH_TABLE *htable,void *key,int size)
{
    return bp_hash_get_value(htable,key,size);
}

unsigned long **bp_hash_int_get_key(BP_HASH_TABLE *htable,unsigned long key)
{
    return (unsigned long **)bp_hash_get_key(htable,&key,sizeof(unsigned long));
}

char **bp_hash_string_get_key(BP_HASH_TABLE *htable,char *key)
{
    return (char **)bp_hash_get_key(htable,key,(bp_gen__strlen(key)+1));
}

void **bp_hash_get_key(BP_HASH_TABLE *htable,void *key,int size)
{
    unsigned long hash_nbr,index;
    int len,page_nbr,j,i,k;
    bp_slist *current_page,*tmplist;
    HASH_PAGE *hpage;
    HASH_KEY_VALUE *newdata;

    len = size;
    if (len <= 0)
        return NULL;
    hash_nbr = bp_hash_nbr_to_table_size(htable,key,len);
    page_nbr = hash_nbr/HASH_PAGE_SIZE;
    current_page = bp_slist__index(htable->datapages,page_nbr);
    hpage = (HASH_PAGE *)current_page->data;
    index = hash_nbr-(page_nbr*HASH_PAGE_SIZE);
    if (hpage->hash_nbr[index] == NO_KEY)
        return NULL;
    j = bp_hash_cmpkeys(htable,hpage->data[index].key,key);
    if (j == 0)
        return &hpage->data[index].key;
    i = bp_slist__length(hpage->data[index].collision);
    if (i)
    {
        for (j=0;j<i;j++)
        {
            tmplist = bp_slist__index(hpage->data[index].collision,j);
            newdata = (HASH_KEY_VALUE *)tmplist->data;
            k = bp_hash_cmpkeys(htable,newdata->key,key);
            if (k == 0)
                return &newdata->key;
        }
    }
    return NULL;
}

void *bp_hash_get_value(BP_HASH_TABLE *htable,void *key,int size)
{
    unsigned long hash_nbr,index;
    int len,page_nbr,j,i,k;
    bp_slist *current_page,*tmplist;
    HASH_PAGE *hpage;
    HASH_KEY_VALUE *newdata;

    len = size;
    if (len <= 0)
        return NULL;
    hash_nbr = bp_hash_nbr_to_table_size(htable,key,len);
    page_nbr = hash_nbr/HASH_PAGE_SIZE;
    current_page = bp_slist__index(htable->datapages,page_nbr);
    hpage = (HASH_PAGE *)current_page->data;
    index = hash_nbr-(page_nbr*HASH_PAGE_SIZE);
    if (hpage->hash_nbr[index] == NO_KEY)
        return NULL;
    j = bp_hash_cmpkeys(htable,hpage->data[index].key,key);
    if (j == 0)
        return hpage->data[index].value;
    i = bp_slist__length(hpage->data[index].collision);
    if (i)
    {
        for (j=0;j<i;j++)
        {
            tmplist = bp_slist__index(hpage->data[index].collision,j);
            newdata = (HASH_KEY_VALUE *)tmplist->data;
            k = bp_hash_cmpkeys(htable,newdata->key,key);
            if (k == 0)
                return newdata->value;
        }
    }
    return NULL;
}

/*

    Delete the key/value pair from the hash table

    parameters:
        htable: the hash table pointer;
        key:    the keyed entry to delete;

    returns 0
*/

int bp_hash_int_entry_delete(BP_HASH_TABLE *ht,unsigned long key)
{
    int ierr;

    ierr = bp_hash_entry_delete(ht,&key);
    return ierr;
}

int bp_hash_string_entry_delete(BP_HASH_TABLE *ht,char *key)
{
    int ierr;

    ierr = bp_hash_entry_delete(ht,key);
    return ierr;
}

int bp_hash_other_entry_delete(BP_HASH_TABLE *ht,void *key)
{
    int ierr;

    ierr = bp_hash_entry_delete(ht,key);
    return ierr;
}

int bp_hash_entry_delete(BP_HASH_TABLE *htable,void *key)
{
    unsigned long hash_nbr,index;
    int len,page_nbr,j,i,k;
    bp_slist *current_page,*tmplist;
    HASH_PAGE *hpage;
    HASH_KEY_VALUE *newdata;

    if (htable->hash_type)
        len = NO_BYTES;
    else
        len = bp_gen__strlen((char *)key)+1;
    hash_nbr = bp_hash_nbr_to_table_size(htable,key,len);
    page_nbr = hash_nbr/HASH_PAGE_SIZE;
    current_page = bp_slist__index(htable->datapages,page_nbr);
    hpage = (HASH_PAGE *)current_page->data;
    index = hash_nbr-(page_nbr*HASH_PAGE_SIZE);
    if (hpage->hash_nbr[index] == NO_KEY)
        return 0;
    j = bp_hash_cmpkeys(htable,hpage->data[index].key,key);
    if (j == 0)
    {
        if (hpage->data[index].vdup)
            bp_hash_free(&hpage->data[index].value);
        if (hpage->data[index].collision)
        {
            tmplist = bp_slist__index(hpage->data[index].collision,0);
            newdata = (HASH_KEY_VALUE *)tmplist->data;
            hpage->data[index].key = newdata->key;
            hpage->data[index].value = newdata->value;
            hpage->data[index].vdup  = newdata->vdup;
            hpage->data[index].ks    = newdata->ks;
            hpage->data[index].vs    = newdata->vs;
            hpage->data[index].collision = bp_slist__remove_link(hpage->data[index].collision,tmplist);
            htable->nbr_entries--;
        }
        else
        {
            hpage->hash_nbr[index] = NO_KEY;
            hpage->data[index].key = NULL;
            hpage->data[index].value = NULL;
            hpage->data[index].collision = NULL;
            hpage->data[index].vdup=0;
            hpage->data[index].ks  =0;
            hpage->data[index].vs  =0;
            htable->nbr_entries--;
        }
    }
    else
    {
        i = bp_slist__length(hpage->data[index].collision);
        for (j=0;j<i;j++)
        {
            tmplist = bp_slist__index(hpage->data[index].collision,0);
            newdata = (HASH_KEY_VALUE *)tmplist->data;
            k = bp_hash_cmpkeys(htable,newdata->key,key);
            if (k == 0)
            {
                if (newdata->vdup)
                    bp_hash_free(&newdata->value);
                hpage->data[index].collision = bp_slist__remove_link(hpage->data[index].collision,tmplist);
                htable->nbr_entries--;
                j=i+1;
            }
        }
    }
    return 0;
}

int bp_hash_cmpkeys(BP_HASH_TABLE *htable,void *key1,void *key2)
{
    int ret=1;
    unsigned long *deref1,*deref2;

    if (htable->hash_type)
    {
        if (htable->hash_type == BP_HASH_OTHER)
        {
            if (key1 == key2)
                return 0;
        }
        else
        {
            deref1 = key1;
            deref2 = key2;
            if (*deref1 == *deref2)
                ret = 0;
        }
    }
    else
    {
        ret = strcmp((char *)key1,(char *)key2);
    }
    return ret;
}


        
unsigned long bp_hash_nbr_to_table_size(BP_HASH_TABLE *htable,void *key,int len)
{
    unsigned long hash_nbr;
    unsigned long *testint;

    if (htable->hash_type)
    {
        testint = (unsigned long *)key;
        if (*testint < htable->max_entries)
            return *testint;
    }  
    hash_nbr = bp_hash_reckon_nbr(key,len);
    hash_nbr = hash_nbr % htable->max_entries;
    if (hash_nbr >= htable->max_entries)
    {
        /* this shold never happen but just checking */
        while (hash_nbr >= htable->max_entries)
            hash_nbr--;
    }
    return hash_nbr;
}


/* Fowler/Noll/Vo hash algorithm
  idea sent by email to the IEEE Posix mailing list
*/


unsigned long bp_hash_reckon_nbr(void *key,int ilen)
{
    unsigned char *last,*tmpkey;
    unsigned long hash_nbr;
    unsigned int itmp;

    last = (unsigned char *)key+ilen;
    tmpkey = (unsigned char *)key;
    for (hash_nbr=0;tmpkey<last;tmpkey++)
    {
        hash_nbr *= HASH_SEED;
        hash_nbr ^= (unsigned long)*tmpkey;
    }
    if (sizeof(unsigned long) > 4)
    {
        itmp = (unsigned int)hash_nbr;    
        hash_nbr = itmp;
    }
    return hash_nbr;
}

char *bp_hash_strdup(char *str)
{
    char *tmp=NULL;

    tmp=(char *)malloc(bp_gen__strlen(str)+1);
    if (str)
       strcpy(tmp,str);
    else
       tmp[0] = '\0';
    return tmp;
}


BP_HASH_SEARCH *bp_hash_search_init(BP_HASH_TABLE *htable)
{

    BP_HASH_SEARCH *hsearch;

    hsearch = (BP_HASH_SEARCH *)malloc(sizeof(BP_HASH_SEARCH));
    if (!hsearch)
        return NULL;
    hsearch->hashtable = htable;
    hsearch = bp_hash_search_rewind(hsearch);
    return hsearch;
}


BP_HASH_SEARCH *bp_hash_search_rewind(BP_HASH_SEARCH *hsearch)
{
    hsearch->thispage  = 0;
    hsearch->index     = 0;
    hsearch->colindex  = -1;
    hsearch->currentpage = NULL;
    hsearch->collision   = NULL;
    hsearch->kvpair      = NULL;
    return hsearch;
}

int bp_hash_search_fin(BP_HASH_SEARCH **hsearch)
{
    bp_hash_free((void **)hsearch);
    return 0;
}

unsigned long bp_hash_search_int_firstkey(BP_HASH_SEARCH *hsearch)
{
    unsigned long *ltmp;

    ltmp = (unsigned long *)bp_hash_search_firstkey(hsearch);
    if (*ltmp == NO_KEY)
    {
        bp_hash_free((void **)&ltmp);
        return -1;
    }
    return *ltmp;
}

char *bp_hash_search_string_firstkey(BP_HASH_SEARCH *hsearch)
{
    char *ret;
    ret = (char *)bp_hash_search_firstkey(hsearch);
    return ret;
}

void *bp_hash_search_other_firstkey(BP_HASH_SEARCH *hsearch)
{
    void *ret;
    unsigned long *ltmp;
    ret = bp_hash_search_firstkey(hsearch);
    ltmp = (unsigned long *)ret;
    if (*ltmp == NO_KEY)
    {
        bp_hash_free((void **)&ret);
        return (void *)NULL;
    }
    return ret;
}


void *bp_hash_search_firstkey(BP_HASH_SEARCH *hsearch)
{
    bp_slist *pagelist;
    HASH_PAGE *hp;
    int type;
    unsigned int i,notfound,thispage;
    unsigned long *lret; 

    if (!hsearch)
        return NULL;
    pagelist = hsearch->hashtable->datapages;
    type = hsearch->hashtable->hash_type;
    if (type)
    {
        lret = (unsigned long *)malloc(sizeof(unsigned long));
        *lret = NO_KEY;
    } 
    if (!pagelist)
    {
        if (type)
            return (void *)lret;
        else
            return (void *)NULL;
    }
    hp = (HASH_PAGE *)pagelist->data;
    notfound = 1;
    thispage = 0;
    while (notfound)
    {
        pagelist = bp_slist__index(hsearch->hashtable->datapages,thispage);
        if (!pagelist)
        {
            if (type)
                return (void *)lret;
            else
                return (void *)NULL;
        }
        hp = (HASH_PAGE *)pagelist->data;
        i = 0;
        do
        {
            if (hp->hash_nbr[i] != NO_KEY)
            {
            hsearch->thispage  = thispage;
            hsearch->index     = i;
            hsearch->colindex  = -1;
            hsearch->currentpage = hp;
            hsearch->collision   = NULL;
            hsearch->kvpair      = &hp->data[i];
            notfound = 0;
            }
            i++;
        } while (notfound && i < HASH_PAGE_SIZE);
        thispage++;
    }
    if (type)
        bp_hash_free((void **)&lret); 
    return hsearch->kvpair->key;
}


unsigned long bp_hash_search_int_nextkey(BP_HASH_SEARCH *hsearch)
{
    unsigned long *ltmp;

    ltmp = (unsigned long *)bp_hash_search_nextkey(hsearch);
    if (*ltmp == NO_KEY)
    {
        bp_hash_free((void **)&ltmp);
        return -1;
    }
    return *ltmp;
}

char *bp_hash_search_string_nextkey(BP_HASH_SEARCH *hsearch)
{
    char *ret;
    ret = (char *)bp_hash_search_nextkey(hsearch);
    return ret;
}

void *bp_hash_search_other_nextkey(BP_HASH_SEARCH *hsearch)
{
    void *ret;
    unsigned long *ltmp;
    ret = bp_hash_search_nextkey(hsearch);
    ltmp = (unsigned long *)ret;
    if (*ltmp == NO_KEY)
    {
        bp_hash_free((void **)&ret);
        return (void *)NULL;
    }
    return ret;
}


void *bp_hash_search_nextkey(BP_HASH_SEARCH *hs)
{
    bp_slist *pagelist,*clist;
    HASH_PAGE *hp;
    int type;
    unsigned long *lret; 
    unsigned int i,notfound,thispage;


    type = hs->hashtable->hash_type;
    if (type)
    {
        lret = (unsigned long *)malloc(sizeof(unsigned long));
        *lret = NO_KEY;
    } 
    pagelist = hs->hashtable->datapages;
    if (!pagelist)
    {
        if (type)
            return (void *)lret;
        else 
            return (void *)NULL;
    }
    notfound = 1;
    if (hs->colindex == (-1))
        hs->collision = hs->kvpair->collision;
    if (hs->collision)
    {
        hs->colindex++;
        clist = bp_slist__index(hs->collision,hs->colindex);
        if (clist)
        {
            notfound = 0;
            hs->kvpair = (HASH_KEY_VALUE *)clist->data;
        }
        else
        {
            hs->colindex = (-1);
        }
    }
    thispage = hs->thispage;
    i = hs->index+1;
    if (i >= HASH_PAGE_SIZE)
    {
        i=0;
        thispage++;
    }
    while (notfound)
    {
        pagelist = bp_slist__index(hs->hashtable->datapages,thispage);
        if (!pagelist)
        {
            if (type)
                return (void *)lret;
            else
                return NULL;
        }
        hp = (HASH_PAGE *)pagelist->data;
        do
        {
            if (hp->hash_nbr[i] != NO_KEY)
            {
            hs->thispage  = thispage;
            hs->index     = i;
            hs->colindex  = -1;
            hs->currentpage = hp;
            hs->collision   = NULL;
            hs->kvpair      = &hp->data[i];
            notfound = 0;
            }
            i++;
        } while (notfound && i < HASH_PAGE_SIZE);
        i=0;
        thispage++;
    }
    if (type)
        bp_hash_free((void **)&lret); 
    return hs->kvpair->key;
}

       
void *bp_hash_search_value(BP_HASH_SEARCH *hs)
{
    return hs->kvpair->value;
}
 
BP_HASH_TABLE *bp_hash_resize(BP_HASH_TABLE **ht,unsigned long sizetogrow)
{

    BP_HASH_TABLE *newht,*oldht;
    BP_HASH_SEARCH *hs;
    void *value;
    unsigned long cursize,newsize,i;
    char *errmsg,*p;
    void *pret;

    oldht = *ht;

    if (!*ht)
        return NULL;
    cursize = oldht->max_entries;
    if (cursize < sizetogrow)
        newsize = sizetogrow;
    else
        newsize = cursize+sizetogrow;

    newht = bp_hash_table_init(newsize,oldht->hash_type);
    hs = bp_hash_search_init(oldht);
    if (!hs)
        return oldht;
    if (oldht->hash_type)
    {
        if (oldht->hash_type == BP_HASH_OTHER)
        {
            for (pret=bp_hash_search_other_firstkey(hs);pret!=(void *)NULL;pret=bp_hash_search_other_nextkey(hs))
            {
                value = bp_hash_search_value(hs);
                if (hs->kvpair->vdup)
                    errmsg = bp_hash_other_entry_dup(newht,pret,hs->kvpair->ks,value,hs->kvpair->vs);
                else
                    errmsg = bp_hash_other_entry(newht,pret,hs->kvpair->ks,value);
                if (errmsg)
                    {
                        bp_hash_search_fin(&hs);
                        bp_hash_table_fin(&newht);
                        printf("%s\n",errmsg);
                        free((char *)errmsg);
                        return oldht;
                    }
            }
        }
        else
        {
            for (i=bp_hash_search_int_firstkey(hs);i != NO_KEY;i=bp_hash_search_int_nextkey(hs))
            {
                value = bp_hash_search_value(hs);
                if (hs->kvpair->vdup)
                    errmsg = bp_hash_int_entry_dup(newht,i,value,hs->kvpair->vs);
                else
                    errmsg = bp_hash_int_entry(newht,i,value);
                if (errmsg)
                {
                    bp_hash_search_fin(&hs);
                    bp_hash_table_fin(&newht);
                    printf("%s\n",errmsg);
                    free((char *)errmsg);
                    return oldht;
                }
            }
        }
    }
    else
    {
        for (p=bp_hash_search_string_firstkey(hs);p != NULL;p=bp_hash_search_string_nextkey(hs))
        {
            value = bp_hash_search_value(hs);
            if (hs->kvpair->vdup)
                errmsg = bp_hash_string_entry_dup(newht,p,value,hs->kvpair->vs);
            else
                errmsg = bp_hash_string_entry(newht,p,value);
            if (errmsg)
            {
                bp_hash_search_fin(&hs);
                bp_hash_table_fin(&newht);
                printf("%s\n",errmsg);
                free((char *)errmsg);
                return oldht;
            }
        }
    }
    bp_hash_search_fin(&hs);
    bp_hash_table_fin(ht);
    return newht;
}



int bp_hash_free(void **fmem)
{

    if (fmem)
    {
        if (*fmem)
        {
            free(*fmem);
            *fmem = NULL;
        }
        fmem = (void *)NULL;
    }
    return 0;
}



