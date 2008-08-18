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

#ifndef BP_HASH_H
#define BP_HASH_H 1
#define HASH_SEED 0x1000193
#define HASH_PAGE_SIZE 256
#define BP_HASH_STRING 0
#define BP_HASH_INT    1
#define BP_HASH_OTHER  2
#define NO_KEY         0xFFFFFFFF
#define NO_BYTES       4
#include "bp_slist_utility.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* 
   hash table 
       hash_type: either BP_HASH_STRING or BP_HASH_INT or BP_HASH_OTHER
                  for is the key int or char * or userdefined (other)
                  Note: the key is duplicated for STRING and INT but
                        not for OTHER
       nbr_entries:  current entries in table
       nbr_pages:    max_entries/HASH_PAGE_SIZE
       max_entries:  max entries allowed in table (multiple of HASH_PAGE_SIZE)
       datapages:    list of HASH_PAGE (HASH_PAGE_SIZE entries per page)
       lmem:         list of memory to free
*/      


typedef struct
{
     int hash_type;
     unsigned long nbr_entries;
     unsigned long nbr_pages;
     unsigned long max_entries;
     bp_slist *datapages;
     bp_slist *lmem;
} BP_HASH_TABLE;


/*

    key/value pairs
        key:    hash key
        value:  hash value
        ks:     size of hash key
        vs:     size of hash value
        vdup:   boolean true if value is to be allocated
        collision: list of HASH_KEY_VALUES with same hash key
*/

typedef struct
{
    void *key;
    void *value;
    int ks;
    int vs;
    int vdup;
    bp_slist *collision;
}  HASH_KEY_VALUE;


/*
   hash page
       this_hash_page:  page identification
       hash_nbr[256]:   hash identification
       data[256]:       key/value pairs
*/


typedef struct
{
    unsigned int this_hash_page;
    unsigned long hash_nbr[HASH_PAGE_SIZE];
    HASH_KEY_VALUE data[HASH_PAGE_SIZE];
} HASH_PAGE;


/*
    hash search/iteration
        thispage:      last datapage number of BP_HASH_TABLE	
        index:         last index of HASH_PAGE 
        int colindex:  index into collision list for same hash number
        currentpage:   current HASH_PAGE
        collision:     list for collision for same hash number
        kvpair:        current key/value pairs

    this token is provided to iterate through the hash table
*/

typedef struct
{
    unsigned int thispage;
    unsigned int index;
    int colindex;
    BP_HASH_TABLE *hashtable;
    HASH_PAGE *currentpage;
    bp_slist *collision;
    HASH_KEY_VALUE *kvpair;
} BP_HASH_SEARCH;

/*  
    initialize a hash table
        entries:  the number of entries requested.  
        type:     BP_HASH_INT or BP_HASH_STRING 
    returns a hash table large enough to handle the requested entries
*/
extern BP_HASH_TABLE *bp_hash_table_init(int entries,int type);

/* 
    finialize a hash table
        frees all unused memeory
    return 0
*/
extern int bp_hash_table_fin(BP_HASH_TABLE **hashtable);

/*
    entry prototypes
        nomanclature for entry/delete/retrieval functions
        example: bp_hash_int_get_value
            bp_hash   - source file for function
            int       - the key type, (can be int or string or other)
            get_value - operation
            _dup      - make a copy of the value



    add/edit a key value pair to the hash table
        parameters:
            htable:  the hash table to update
            key:     the key
            value:   the value
            size:    required for the dup functions to make a copy of the value

        if the key already exist in the hash table the value 
        will be update otherwise the entry will be added

    returns a NULL on success otherwise a error message is returned

    these functions call the private function:
        char *bp_hash_entry(BP_HASH_TABLE *htable,
                            void *key,
                            int keysize,
                            void *value,
                            int valuesize,int vdup);

*/

extern char *bp_hash_int_entry(BP_HASH_TABLE *htable,unsigned long key,void *value);
extern char *bp_hash_string_entry(BP_HASH_TABLE *htable,char *key,void *value);
extern char *bp_hash_int_entry_dup(BP_HASH_TABLE *htable,unsigned long key,void *value,int size);
extern char *bp_hash_string_entry_dup(BP_HASH_TABLE *htable,char *key,void *value,int size);
extern void *bp_hash_other_entry(BP_HASH_TABLE *htable,void *key,int keysize,void *value);
extern void *bp_hash_other_entry_dup(BP_HASH_TABLE *htable,void *key,int keysize,void *value,int size);

/*
    delete a key value pair from the hash table
        parameters:
            htable: the hash table to update
            key:    the key for deletion

    These functions call the private function:
        int bp_hash_entry_delete(BP_HASH_TABLE *htable,void *key)

    returns 0
*/

extern int bp_hash_int_entry_delete(BP_HASH_TABLE *htable,unsigned long key);
extern int bp_hash_string_entry_delete(BP_HASH_TABLE *htable,char *key);
extern int bp_hash_other_entry_delete(BP_HASH_TABLE *htable,void *key);

/*
    retrieve values from the hash table
        parameters:
            htable: the hash table to use
            key:    the key for search
            size:   for other key type

    These functions call the private function:
        int bp_hash_get_value(BP_HASH_TABLE *htable,void *key)

    returns a void pointer to the value which must be cast to the
    appropriate return type

    the get_key methods return the address of the key for memory management.
    These methods should only be needed for globally scoped hash tables of
    types BP_HASH_INT and BP_HASH_STRING that are constantly being updated. 
    The keys should be freed with bp_hash_free. Hash type BP_HASH_OTHER
    does not need the get_key method as the key is not duplicated.


*/    
    
extern void *bp_hash_int_get_value(BP_HASH_TABLE *htable,unsigned long key);
extern void *bp_hash_string_get_value(BP_HASH_TABLE *htable,char *key);
extern void *bp_hash_other_get_value(BP_HASH_TABLE *htable,void *key,int size);
extern unsigned long **bp_hash_int_get_key(BP_HASH_TABLE *htable,unsigned long key);
extern char **bp_hash_string_get_key(BP_HASH_TABLE *htable,char *key);


/* 
    search/iteration of hash 
        nomanclature for search iteration functions
        example: bp_hash_search_int_firstkey
            bp_hash   - source file for function
            search    - iteration mode
            int       - the key type (can be int or string)
            firstkey  - operation
    search operations
       _init:     return a search token to be used for iteration
       _rewind:   rewind the search token to the top of the hash
       _fin:      destory search token

    interation opereations
       _firstkey:  returns the first key of the hash table
       _nextkey:   returns the next key in the hash table
       _value:     return the value of the last iteration

    special values
       NO_KEY:  used for integer key interations

    example of use for strings

        hashsearch = bp_hash_search_init(hashtable);
        for (key = bp_hash_search_string_firstkey(hashsearch);
             key != NULL;
             key = bp_hash_search_string_nextkey(hashsearch))
        {
            value = (char *)bp_hash_search_value(hashsearch);
            printf("%s %s\n",key,value);
        }
        bp_hash_search_fin(&hashsearch);

   example of use for integer

        hashsearch = bp_hash_search_init(hashtable);
        for (key = bp_hash_search_int_firstkey(hashsearch);
             key != NO_KEY;
             key = bp_hash_search_int_nextkey(hashsearch))
        {
            value = (char *)bp_hash_search_value(hashsearch);
            printf("%ld %s\n",key,value);
        }
        bp_hash_search_fin(&hashsearch);


   
*/

/* search */
extern BP_HASH_SEARCH *bp_hash_search_init(BP_HASH_TABLE *htable);
extern BP_HASH_SEARCH *bp_hash_search_rewind(BP_HASH_SEARCH *hsearch);
extern int bp_hash_search_fin(BP_HASH_SEARCH **hsearch);


/* iteration */
extern unsigned long bp_hash_search_int_firstkey(BP_HASH_SEARCH *hsearch);
extern char *bp_hash_search_string_firstkey(BP_HASH_SEARCH *hsearch);
extern void *bp_hash_searck_other_firstkey(BP_HASH_SEARCH *hsearch);

extern unsigned long bp_hash_search_int_nextkey(BP_HASH_SEARCH *hsearch);
extern char *bp_hash_search_string_nextkey(BP_HASH_SEARCH *hsearch);
extern void *bp_hash_search_other_nextkey(BP_HASH_SEARCH *hsearch);


extern void *bp_hash_search_value(BP_HASH_SEARCH *hs);

/*

   resize the hash table

   if on _entry an error message of 
   "Hash table is full" is returned 
   you may resize the table.

*/

extern BP_HASH_TABLE *bp_hash_resize(BP_HASH_TABLE **ht,unsigned long sizetogrow);
extern int bp_hash_free(void **fmem);

#if defined(__cplusplus)
}
#endif

#endif
