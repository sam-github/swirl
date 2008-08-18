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
 * $Id: bp_config.c,v 1.5 2002/03/27 08:53:40 huston Exp $
 *
 * IW_config.c
 *
 * Functionality for parsing the configuration file.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifndef WIN32
#include <unistd.h>
#endif
#include "semutex.h"


#include "../../utility/xml_parse_constants.h"
#include "../../utility/bp_malloc.h"
#include "../../utility/bp_hash.h"
#include "bp_config.h"


/*
 * Config struct
 */
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

/*
 * Search struct
 */
typedef struct cfgsearchobj {
    struct cfgsearchobj *next;
    BP_HASH_TABLE *config_options;
    BP_HASH_SEARCH *hsearch;
    char searchstate;
    char *headstr;
    int headstr_len;
    char *substr;
} cfgsearchobj;

#define SEARCH_REWOUND          (char)0
#define SEARCH_ACTIVE           (char)1
#define SEARCH_EOF              (char)2

/*
 * local proto's
 */
int config_parse_elem(struct configobj *, char **);
int config_parse_cdata(struct configobj *cfg, char **current, char **cdatabuf);
char *realloc_string(char *inbuf, int size);
/*
 * config_init
 * config_fin
 *
 * basic setup we check for everywhere else.
 *
 * Cleanup when we are done, if called...
 */
struct configobj *config_new(struct configobj *defaults) {
    struct configobj *cfg;

    if ((cfg = lib_malloc(sizeof(struct configobj)))) {
        memset(cfg, 0, sizeof(struct configobj));
#ifdef WIN32
        cfg->sem_config = CreateMutex(NULL, FALSE, NULL);
        if (cfg->sem_config == NULL) {
            lib_free(cfg);
            return NULL;
        }
#else
        SEM_INIT(&cfg->sem_config,1);
#endif
        cfg->keybuf_size = 128;
        cfg->config_options = bp_hash_table_init(256, BP_HASH_STRING);
        cfg->defaults = defaults;
    }
    
    return cfg;
}

void config_destroy(configobj *cfg)
{
    if (cfg) {
#ifdef WIN32
        CloseHandle(cfg->sem_config);
#else
        SEM_WAIT(&cfg->sem_config);

        SEM_POST(&cfg->sem_config);
        SEM_DESTROY(&cfg->sem_config);
#endif

        bp_hash_table_fin(&cfg->config_options);
        lib_free(cfg);
    }

    return;
}

/*
 *
 */
#ifdef WIN32
int config_parse_file(configobj *cfg, char *filename)
{
    HANDLE fd;
    DWORD size;
    DWORD bytesRead = 0;
    DWORD n;
    char *filebuf;
    int retval;

    fd = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (fd == INVALID_HANDLE_VALUE) {
        printf("Error on open file %s\n", filename);
        return CONFIG_FILE_ERROR;
    } 

    size = GetFileSize(fd, NULL);
    if (size == INVALID_FILE_SIZE) {
        printf("Error on stat file %s\n", filename);
        return CONFIG_FILE_ERROR;
    }

    filebuf = malloc(size + 1);
    if (NULL == filebuf) {
        printf("Error: malloc failed for %d bytes.\n", size);
        return CONFIG_ALLOC_FAIL;
    }

    do {
        if (ReadFile(fd, &filebuf[bytesRead], size, &n, NULL) == FALSE) {
            printf("Error on read file %s\n", filename);
            lib_free(filebuf);
            return CONFIG_FILE_ERROR;
        }
        bytesRead += n;
    } while (bytesRead < size && n != 0);

    filebuf[size] = '\0';

    retval = config_parse(cfg, filebuf);

    lib_free(filebuf);
    return retval;
}
#else
int config_parse_file(configobj *cfg, char *filename)
{  
    int fd;
    int x, retval = CONFIG_OK;
    struct stat statbuf;
    char *filebuf;

    if (stat(filename, &statbuf)) {
/*      printf("Error on stat file %s\n", filename); */
        return CONFIG_FILE_ERROR;
    } 

    if ((int)statbuf.st_size) {
        if (NULL== (filebuf = malloc(statbuf.st_size + 1))) {
/*          printf("Error: malloc failed for %d bytes.\n", (int)statbuf.st_size); */
            return CONFIG_ALLOC_FAIL;
        }

        if (0 == (fd = open(filename, O_RDONLY))) {
/*          printf("Error on open file %s\n", filename); */
            lib_free(filebuf);
            return CONFIG_FILE_ERROR;
        }

        if (0 == (x = read(fd, filebuf, (int)statbuf.st_size))) {
/*          printf("Error on read file %s\n", filename); */
            lib_free(filebuf);
            return CONFIG_FILE_ERROR;
        }

        filebuf[(int)statbuf.st_size] = (char)0;
        retval = config_parse(cfg, filebuf);
        lib_free(filebuf);
    }

    return retval;
}
#endif

/*
 *
 */
int config_parse(configobj *cfg, char *cfgbuf)
{  
    char *current;

    current = cfgbuf;

#ifdef WIN32
    if (WaitForSingleObject(cfg->sem_config, INFINITE) == WAIT_FAILED) {
        return CONFIG_NOT_INITIALIZED;
    }
#else
    SEM_WAIT(&cfg->sem_config);
#endif

    if (NULL == (cfg->keybuf = lib_malloc(cfg->keybuf_size))) {
#ifdef WIN32
        ReleaseMutex(cfg->sem_config);
#else
        SEM_POST(&cfg->sem_config);
#endif
        return CONFIG_ALLOC_FAIL;
    }

    memset(cfg->keybuf, 0, cfg->keybuf_size);

    while ('<' != *current)
        current++;

    config_parse_elem(cfg, &current);

    lib_free(cfg->keybuf);
#ifdef WIN32
    ReleaseMutex(cfg->sem_config);
#else
    SEM_POST(&cfg->sem_config);  
#endif
    return CONFIG_PARSE_OK;
}

/*
 *
 * Note here that the hash table manages the memory properly for duplicate entries.
 */
int config_set(configobj *cfg, char *elem, char *value) 
{
    char * error = NULL;
    int retcode = CONFIG_OK;

#ifdef WIN32
    if (WaitForSingleObject(cfg->sem_config, INFINITE) == WAIT_FAILED) {
        return CONFIG_NOT_INITIALIZED;
    }
    
#else
    SEM_WAIT(&cfg->sem_config);
#endif

    /* $@$@$@$ need error handling here */
    if ((error = bp_hash_string_entry_dup(cfg->config_options, elem, value, strlen(value) +1))) {
        lib_free(error);
        cfg->config_options = bp_hash_resize(&(cfg->config_options), 1);
        error = bp_hash_string_entry_dup(cfg->config_options, elem, value, strlen(value) +1);
        if (error) {
            retcode = CONFIG_ALLOC_FAIL;
            lib_free(error);
        }
    }

#ifdef WIN32
    ReleaseMutex(cfg->sem_config);
#else
    SEM_POST(&cfg->sem_config);  
#endif

    return retcode;
}

/*
 *
 * Note here that the hash table manages the memory properly for duplicate entries.
 */
int config_test_and_set(configobj *cfg, char *elem, char *value) 
{
    char *tmp = NULL;
    char *error = NULL;
    int retcode = CONFIG_OK;

#ifdef WIN32
    if (WaitForSingleObject(cfg->sem_config, INFINITE) == WAIT_FAILED) {
        return CONFIG_NOT_INITIALIZED;
    }    
#else
    SEM_WAIT(&cfg->sem_config);
#endif

    /* $@$@$@$ need error handling here */

    tmp = bp_hash_string_get_value(cfg->config_options, elem);

    if (tmp) {
        retcode = CONFIG_ALREADY_SET;
    } else {
        if ((error = bp_hash_string_entry_dup(cfg->config_options, elem, value, strlen(value) +1))) {
            lib_free(error);
            cfg->config_options = bp_hash_resize(&(cfg->config_options), 1);
            error = bp_hash_string_entry_dup(cfg->config_options, elem, value, strlen(value) +1);
            if (error) {
                retcode = CONFIG_ALLOC_FAIL;
                lib_free(error);
            }
        }
    }

#ifdef WIN32
    ReleaseMutex(cfg->sem_config);
#else
    SEM_POST(&cfg->sem_config);  
#endif

    return retcode;
}

int config_delete(configobj *cfg, char *elem) 
{
#ifdef WIN32
    if (WaitForSingleObject(cfg->sem_config, INFINITE) == WAIT_FAILED) {
        return CONFIG_NOT_INITIALIZED;
    }
    
#else
    SEM_WAIT(&cfg->sem_config);
#endif

    bp_hash_string_entry_delete(cfg->config_options, elem);

#ifdef WIN32
    ReleaseMutex(cfg->sem_config);
#else
    SEM_POST(&cfg->sem_config);  
#endif

    return CONFIG_OK;
}

char *config_get(configobj *cfg, char *elem) 
{
    char *tmp = NULL;

    tmp = bp_hash_string_get_value(cfg->config_options, elem);

    if ((NULL == tmp) && cfg->defaults)
	return config_get(cfg->defaults, elem);
    else
	return tmp;
}

/*
 * search functionality.
 */
cfgsearchobj *config_search_init(configobj *cfg, 
					   char * head, char *sub)
{
    struct cfgsearchobj *tmp;

    if ((tmp = lib_malloc(sizeof (struct cfgsearchobj)))) {
        memset(tmp, 0, sizeof (struct cfgsearchobj));
        tmp->hsearch = bp_hash_search_init(cfg->config_options);

        if (head) {
            tmp->headstr = head;
            tmp->headstr_len = strlen(head);
        }

        if (sub) {
            tmp->substr = sub;
        }
	
        tmp->config_options = cfg->config_options;

        /* recurse */
        if (cfg->defaults) 
            tmp->next = config_search_init(cfg->defaults, head, sub);
    }

    return tmp;
}

cfgsearchobj *config_search_rewind(cfgsearchobj *csearch)
{
    bp_hash_search_rewind(csearch->hsearch);
    csearch->searchstate = SEARCH_REWOUND;
    
    if (csearch->next)
        config_search_rewind(csearch->next);

    return csearch;
}

int config_search_fin(cfgsearchobj **csearch)
{
    /* wimmin 'n children first */
    if ((*csearch)->next)
        config_search_fin(&((*csearch)->next));

    /* now its our turn */
    bp_hash_search_fin(&((*csearch)->hsearch));
    lib_free(*csearch);
    *csearch = NULL;

    return 0;
}

int cfg_search_match (cfgsearchobj *csearch, char * key)
{
    int a = 1, b = 1;

    if (!key)
        return 0;

    if (csearch->headstr) {
        a = strncmp(key, csearch->headstr, csearch->headstr_len);
    } else {
        a = 0;
    }

    if (csearch->substr) {
        b = !((int)strstr(key, csearch->substr));
    } else {
        b = 0;
    }

    return (!a && !b);
}

char *config_search_string_firstkey(cfgsearchobj *csearch)
{
    char * tmp;

    config_search_rewind(csearch);

    tmp = bp_hash_search_string_firstkey(csearch->hsearch);

    if (tmp)
        csearch->searchstate = SEARCH_ACTIVE;
    else 
        csearch->searchstate = SEARCH_EOF;	

    if (cfg_search_match (csearch, tmp)) {
        return tmp;
    } else {
        if (tmp || csearch->next)
            return config_search_string_nextkey(csearch);
        else
            return tmp;
    }
}

char *config_search_string_nextkey(cfgsearchobj *csearch){
    char * tmp;

    switch (csearch->searchstate) {
    case SEARCH_ACTIVE:
        while ((tmp = bp_hash_search_string_nextkey(csearch->hsearch))) {
            if (tmp) {
                if (cfg_search_match (csearch, tmp))
                    return tmp;
            }
        }
        csearch->searchstate = SEARCH_EOF;
        if (csearch->next) {
            return config_search_string_firstkey(csearch->next);
        } else {
            return NULL;
        }
        break;
    case SEARCH_EOF:
        if (csearch->next) {
            while ((tmp= config_search_string_nextkey(csearch->next))) {
                if (NULL == bp_hash_string_get_value(csearch->config_options, tmp))
                    return tmp;
            }
        }
        break;
    default:
        return NULL;
        break;
    }

    return NULL;
}


char *config_search_value(cfgsearchobj *csearch) 
{
    switch (csearch->searchstate) {
    case SEARCH_ACTIVE:
        return (char *)bp_hash_search_value(csearch->hsearch);
        break;
    case SEARCH_EOF:
        if (csearch->next) {
            return config_search_value(csearch->next);
        }
        break;
    default:
        return NULL;
        break;
    }

    /* should never hit here. */
    return NULL;
}

/*
 * local stuff
 */
int config_parse_elem(configobj *cfg, char **current) 
{
    int ecode = CONFIG_PARSE_OK;
    char *error;
    char *tmp, *cdata;
    char *stacktail;
    char *myelemname;
    int x;
    int mynamelen = 0;
    
    /* we get called pointing to '<', lose it */
    (*current)++;
    
    /* skip comments */
    if (0 == strncmp(*current, "comment", 7)) {
        while ('>' != **current) 
            (*current)++;
        if ('/' == (*current)[-1]) {
            (*current)++;
        } else {	    
            if ((tmp = strstr(*current, "</comment"))) {
                *current = tmp + 9;
                while ('>' != **current) 
                    (*current)++;
                (*current)++;
            } else {
                return CONFIG_PARSE_ERROR;
            }
        }
    } else {
        if (XML_NAMESTART & XML_UTF8_charset[(int)**current]) {
            myelemname = *current;

            while (XML_NAME & XML_UTF8_charset[(int)**current]) {
                mynamelen++;
                (*current)++;
            }

            if (!XML_S & XML_UTF8_charset[(int)**current]) 
                return CONFIG_PARSE_ERROR;
	    
	    /* chop off the tail */
	    while ('>' != **current) 
		(*current)++;
	    (*current)++;

	    /* wierd case would be unary close */
	    if ('/' != (*current)[-1]) {
		/* push our name onto the string stack */
		x = strlen(cfg->keybuf);

		if (cfg->keybuf_size < (x + mynamelen + 1)) {
		    cfg->keybuf_size *= 2;
		    if (NULL == (cfg->keybuf = realloc_string(cfg->keybuf, cfg->keybuf_size))) {
			return CONFIG_ALLOC_FAIL;
		    }
		}

		stacktail = cfg->keybuf + x;
		tmp = stacktail;
		if (*cfg->keybuf) {
		    *stacktail = CONFIG_KEY_SEPARATOR;
		    tmp++;
		}
		strncpy(tmp, myelemname, mynamelen);
		tmp[mynamelen] = (char)0;
		
		/* we have an element, get it's cdata */
		if ((ecode = config_parse_cdata(cfg, current, &cdata))) {
		    return ecode;
		} 

		/* 
		 * now check that we close the correct element, the 
		 * cdata parser returns on "</"
		 */
		*current += 2;
		if (strncmp(*current, myelemname, mynamelen)) {
		    return CONFIG_PARSE_ERROR;
		}

		/* finally save off any CDATA if needed, then unwind the stack */
		/* $@$@$@$ ned error handling here */
		if (cdata) {
/*  		    printf("hashing key='%s' value='%s'\n", cfg->keybuf, cdata); */
		    if ((error = bp_hash_string_entry_dup(cfg->config_options, cfg->keybuf, 
					     cdata, strlen(cdata) +1))) {
			lib_free(error);
			cfg->config_options = bp_hash_resize(&(cfg->config_options), 1);
			error = bp_hash_string_entry_dup(cfg->config_options, cfg->keybuf, 
					     cdata, strlen(cdata) +1);
			if (error) {
			    ecode = CONFIG_ALLOC_FAIL;
			    lib_free(error);
			}
		    }
		}

		while (*stacktail)
		    *stacktail = (char)0;
	    }
	} else {
	    ecode = CONFIG_PARSE_ERROR;
	}
    }
    
    if (cdata)
        lib_free(cdata);

    return ecode;
}
/*
 * config_parse_cdata
 *
 * Copies chunks of cdata into the return buffer and calls the 
 * element parser for sub-elements.  Assumes that the current 
 * pointer will be incremented within the config_parse_elem.
 */
int config_parse_cdata(configobj *cfg, char **current, char **cdatabuf) 
{
    int ecode = 0;
    int isleaf = 1;
    int x = 0, y = 0;
    int mybufsize = 64;
    char *mybuf, *mytmp, *mytail;

    if (NULL == (mybuf = lib_malloc(mybufsize))) 
	return CONFIG_ALLOC_FAIL;

    memset(mybuf, 0, mybufsize);
    *cdatabuf = mybuf;
    mytmp = mybuf;

    while (1) {
	if ('<' == **current) {
	    switch ((*current)[1]) {
	    case '/':
		return CONFIG_PARSE_OK;
		break;
	    case '!':
		if (isleaf && (0 == strncmp(*current, "<![CDATA[", 9))) {
		    if (NULL == (mytail = strstr(*current, "]]>"))) {
			ecode = CONFIG_PARSE_ERROR;
			goto error;
		    } else {
			*current += 9;
			y = (int)(mytail - *current);
			if ((mybufsize - x) < y) {
			    mybufsize *= 2;
			    if (NULL == (mybuf = realloc_string(mybuf, mybufsize))) {
				ecode = CONFIG_ALLOC_FAIL;
				goto error;
			    }
			    *cdatabuf = mybuf;
			    mytmp = mybuf + x;
			}
			strncpy(mytmp, *current, y);
			mytmp += y;
			x += y;
			*current += y + 3 - 1;
		    }
		}
		break;
	    default:
		isleaf = 0;
		lib_free(mybuf);
		mybuf = NULL;
		*cdatabuf = NULL;
		if ((ecode = config_parse_elem(cfg, current))) {
		    ecode = CONFIG_ALLOC_FAIL;
		    goto error;
		}
		break;
	    }
	} else {
	    if (NULL == (mytail = strstr(*current, "<"))) {
		ecode = CONFIG_PARSE_ERROR;
		goto error;
	    } else {
		y = (int)(mytail - *current);

		if (isleaf) {
		    if ((mybufsize - x) < y) {
			mybufsize *= 2;
			if (NULL == (mybuf = realloc_string(mybuf, mybufsize))) {
			    ecode = CONFIG_ALLOC_FAIL;
			    goto error;
			}
			*cdatabuf = mybuf;
			mytmp = mybuf + x;
		    }
		    strncpy(mytmp, *current, y);
		}
		mytmp += y;
		x += y;
		*current += y;
	    }
	}
    }

 error:
    if (mybuf)
        lib_free(mybuf);
    return ecode;
}

/*
 * 
 */
char *realloc_string(char *inbuf, int size) 
{
    char *tmp = NULL;

    if ((tmp = lib_malloc(size))) {
        memset(tmp, 0, size);
        strcpy(tmp, inbuf);
        lib_free(inbuf);
    }
    return tmp;
}
