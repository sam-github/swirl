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

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../bp_hash.h"
#include "../bp_slist_utility.h"

extern unsigned long bp_hash_reckon_nbr(void *key,int ilen);


int main(int argc,char *argv[])
{
    int i;
    unsigned long test,hash;
    unsigned long temp[1024];
    unsigned long *retval;
    unsigned int seed=63;
    unsigned int len,j;
    char ctest[256];
    char cbase[66];
    char *errmsg,*p;
    BP_HASH_TABLE *ht;
    BP_HASH_SEARCH *hs;
    for (i=0;i<1024;i++)
        temp[i]=0;

    ht = bp_hash_table_init(30000,BP_HASH_INT);
    
    for (test=0;test<0xFFFF;test++)
    {
        errmsg = bp_hash_int_entry_dup(ht,test,&test,(int)sizeof(unsigned long));
        if (errmsg)
        {
            printf("%s\n",errmsg);
            free((char *)errmsg);
            errmsg=NULL;
            ht = bp_hash_resize(&ht,10000);
            test--;
        }
    }

    retval = (unsigned long *)bp_hash_int_get_value(ht,32002);


    bp_hash_int_entry_delete(ht,32002);

    bp_hash_table_fin(&ht);

    strcpy(cbase,"AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456789+=/");
    
    srand(seed);

    ht = bp_hash_table_init(1,BP_HASH_STRING);


    for (test=0;test<20;test++)
    {
        len=0;
        while (!len)
            len = (unsigned int)(255.0*rand()/(RAND_MAX+1.0));
        for (i=0;i<len;i++)
        {
            j = (int)(64.0*rand()/(RAND_MAX+1.0));
            ctest[i] = cbase[j];
        }
        ctest[len] = '\0';
        printf("set key: %s value %ld\n",ctest,test);
        errmsg = bp_hash_string_entry_dup(ht,ctest,&test,(int)sizeof(unsigned long));
        if (errmsg)
        {
            printf("%s\n",errmsg);
            free((char *)errmsg);
            errmsg=NULL;
        }
    }

    hs = bp_hash_search_init(ht);
    for (p=bp_hash_search_string_firstkey(hs);p != NULL;
         p=bp_hash_search_string_nextkey(hs))
    {
        retval = (unsigned long *)bp_hash_search_value(hs);
        printf("key:%s value:%ld\n",p,*retval);
    }

    bp_hash_search_fin(&hs);
    bp_hash_table_fin(&ht);
    

    for (test=0;test<0xFFFF;test++)
    {
        hash = bp_hash_reckon_nbr(&test,sizeof(unsigned long));
        temp[(hash % 1024)]++;
        printf("count %ld dhash %ld - hash %ld\n",test,hash,(hash % 1024));
    }
    for (i=0;i<1024;i++)
         printf("c %d collision %ld\n",i,temp[i]);
    return 0;
}



