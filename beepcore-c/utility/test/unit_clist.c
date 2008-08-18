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
 * $Id: unit_clist.c,v 1.1 2002/03/28 23:03:10 sweetums Exp $
 *
 * Unit test for the circluar list widget.
 */
#include <stdio.h>
#include <string.h>
#include "../bp_malloc.h"

extern int main(int argc, char ** argv) {
    struct bp_clist * list;
    int i, j, k;
    int N = 4;

    lib_malloc_init(malloc, free);

    
    list = bp_clist_new();

    if (!list) {
	printf("List constructor failed.\n");
	return 1;
    }

    for (i=N; i; i--) {
	if (!bp_clist_add(list, (void*)i)){
	    printf("bp_clist_add failed.\n");
	    return 1;	    
	}
    }

    /* test next */
    j = 0;
    k = 0;
    for (i=N; i; i--) {
	j += i;
	k += (int)bp_clist_next(list);
    }
    if (j != k) {
	printf("bp_clist_next test failed.\n");
	return 1;	    
    }

    /* test find */    
    for (i=N; i; i--) {
	if (!bp_clist_find(list, (void*)i)){
	    printf("bp_clist_find failed.\n");
	    return 1;	    
	}
    }
    
    if (bp_clist_length(list) != N) {
	printf("List node count incorrect.\n");
	return 1;
    }

    for (i=N; i; i--) {
	if (!bp_clist_delete(list)){
	    printf("bp_clist_delete failed.\n");
	    return 1;	    
	}
    }

    for (i=N; i; i--) {
	bp_clist_add(list, (void*)i);
    }
    
    for (i=1; i<=N; i++) {
	if (!bp_clist_remove(list, (void*)i)){
	    printf("bp_clist_remove failed.\n");
	    return 1;	    
	}
    }
    


    if (bp_clist_length(list) == 0) {
	if (bp_clist_delete(list)){
	    printf("bp_clist_delete failed, should return 0 on delete from empty list.\n");
	    return 1;	    
	}
    } else {
	printf("Skipped test of delete on empty list (list is not empty).\n");
    }
    

    printf("\n");

    bp_clist_free(list);

    printf("bp_clist unit tests passed.\n");

    return 0;
}

