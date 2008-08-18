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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "../bp_config.h"
#include "../../../utility/bp_malloc.h"
#include "../../../utility/bp_hash.h"
#include "../semutex.h"

extern unsigned long IW_hash_reckon_nbr(void *key,int ilen);

typedef struct configobj {
    BP_HASH_TABLE *config_options;
    char *keybuf;
    int keybuf_size;
#ifdef WIN32
    HANDLE sem_config;
#else
    sem_t sem_config;
#endif
    struct configobj *defaults;
} configobj;


int parse_file(char * filename, struct configobj **);
int parse_filename(char * filename, struct configobj **);
void dump_string_ht(BP_HASH_TABLE *ht);
void dump_config(struct configobj *cfg);

/*
 *
 */
int main(int argc,char *argv[])
{
    char * foo;
    struct configobj * mycfg, * topcfg;

    lib_malloc_init(malloc, free);

    if ((mycfg = config_new(NULL))) {

	config_set(mycfg, "foo", "bar");

	foo = config_get(mycfg, "foo");
	printf("key='%s' got '%s'\n", "foo", foo);

	foo = config_get(mycfg, "FOO");
	if (foo)
	  printf("key='%s' got '%s'\n", "FOO", foo);
	else 
	  printf("key='%s' got ''\n", "FOO");	  

	config_destroy(mycfg);
    }


    parse_file("sample_config.1", &mycfg);
    config_destroy(mycfg);

    parse_file("sample_config.2", &mycfg);
    /* Now test layering */
    if ((topcfg = config_new(mycfg))) {
	printf("tesing layering... \n");

	config_set(mycfg, "foo", "Set in default layer");
	config_set(mycfg, "bar", "Set only in default layer");

	config_set(topcfg, "baz", "Set only in active layer");
	config_set(topcfg, "foo", "Set in active layer");

	printf("\n\nSet and check\n");
	foo = config_get(topcfg, "foo");
	printf("key='%s' got '%s'\n", "foo", foo);

	foo = config_get(topcfg, "bar");
	printf("key='%s' got '%s'\n", "bar", foo);

	foo = config_get(topcfg, "baz");
	printf("key='%s' got '%s'\n", "baz", foo);

	printf("\n\nDumping from top layer\n");
	
	dump_config(topcfg); 

	config_destroy(topcfg);
    }

    config_destroy(mycfg);

    parse_filename("sample_config.2", &mycfg);
    config_destroy(mycfg);

    return 0;
}



int parse_file(char * filename, struct configobj **newcfg) 
{
    int fd;
    int x;
    struct stat statbuf;
    char *filebuf = NULL;
    struct configobj * mycfg;
    struct cfgsearchobj *mycs;    
    char * srchtmp;

    if (stat(filename, &statbuf)) {
	printf("Error stat on file %s\n", filename);
	return 1;
    } 

    if (NULL== (filebuf = malloc(statbuf.st_size + 1))) {
	printf("Error: malloc failed for %d bytes.\n", (int)statbuf.st_size);
	return 1;
    }

    if (0 == (fd = open(filename, O_RDONLY))) {
	printf("Error on open file %s\n", filename);
	return 1;
    }

    x = read(fd, filebuf, (int)statbuf.st_size);
    filebuf[(int)statbuf.st_size] = (char)0;

    *newcfg = NULL;    
    if ((mycfg = config_new(NULL))) {
        *newcfg = mycfg;
	config_parse(mycfg, filebuf);
	printf("\nDumping has table key/value pairs.\n");
	dump_string_ht(mycfg->config_options);
	printf("\n");

	printf("running a search for first...\n");
	if ((mycs = config_search_init(mycfg, NULL, NULL))) {
	  printf("get first thing got key=");
	  srchtmp = config_search_string_firstkey(mycs);
	  if (srchtmp) {
	    printf("'%s'\n", srchtmp);
	  } else {
	    printf("NULL\n");
	  }
	} else {
	  printf("search init failed\n");
	}

	printf("running a search for string 'uri'...\n");
	if ((mycs = config_search_init(mycfg, NULL, "uri"))) {
	  printf("get first thing got key=");
	  srchtmp = config_search_string_firstkey(mycs);
	  if (srchtmp) {
	    printf("'%s'\n", srchtmp);
	  } else {
	    printf("NULL\n");
	  }
	} else {
	  printf("search init failed\n");
	}

/*   	config_destroy(mycfg); */
    }

    if (fd) 
	close(fd);

    if (filebuf)
	free(filebuf);

    return 0;
}

int parse_filename(char * filename, struct configobj ** newcfg) 
{
    struct configobj * mycfg;

    mycfg = config_new(NULL);
    *newcfg = mycfg;

    config_parse_file(mycfg, filename);
    printf("\nDumping has table key/value pairs.\n");
    dump_string_ht(mycfg->config_options);
    printf("\n");

    /*    config_destroy(mycfg); */

    return 0;
}

void dump_string_ht(BP_HASH_TABLE *ht) 
{
    BP_HASH_SEARCH *hs;
    char *retstr,*p;

    hs = bp_hash_search_init(ht);
    for (p=bp_hash_search_string_firstkey(hs);p != NULL;
         p=bp_hash_search_string_nextkey(hs))
    {
        retstr = (char *)bp_hash_search_value(hs);
        printf("key:'%s' value:'%s'\n", p, retstr);
    }

    bp_hash_search_fin(&hs);
}

void dump_config(struct configobj *cfg) 
{
    struct cfgsearchobj *mysearch;
    char *retstr,*p;

    mysearch = config_search_init(cfg, "", "");

    for (p=config_search_string_firstkey(mysearch);p != NULL;
         p=config_search_string_nextkey(mysearch))
    {
        retstr = (char *)config_search_value(mysearch);
        printf("key:'%s' value:'%s'\n", p, retstr);
    }

    config_search_fin(&mysearch);
}

#include "../../wrapper/bp_wrapper.h"

struct diagnostic* bp_diagnostic_new(struct BP_CONNECTION *conn,
					    int code,
					    char *language,
					    char *fmt, ...) {
    printf ("configtest called bp_diagnostic_new?!?\n");

    return NULL;
}
