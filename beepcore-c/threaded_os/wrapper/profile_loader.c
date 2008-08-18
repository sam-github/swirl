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
 * $Id: profile_loader.c,v 1.12 2002/03/27 08:53:41 huston Exp $
 *
 * profile_loader.c
 *
 * Utilities to load profile libraries at runtime.
 *
 */

#ifndef WIN32
#include <unistd.h>
#include "../utility/semutex.h"
#include <dlfcn.h>
#include <errno.h>
#else
#include <windows.h>
#define sem_t HANDLE
#endif

#include <stdlib.h>
#include <string.h>
#include "profile_loader.h"
#include "../utility/logutil.h"


#define	BEEP_LOAD_PROFILES	"beep %s application %s load_profiles"
#define	BEEP_PRO_LIBFILE	"beep profiles %s libfile"
#define	BEEP_PRO_INITNAME	"beep profiles %s initname"

#ifdef WIN32
#define snprintf _snprintf
#endif

/* the hso (handle to shared object) is not saved so dlclose will never be
   called... */

PROFILE_REGISTRATION *
load_beep_profiles (struct configobj *appconfig,
		    char	     *pgmname,
		    char	     *dataname) {
    int		 size,
                 warnedP;
    char	 buffer[BUFSIZ],
	       **libfiles,
	       **initnames,
	       **pp,
		*value,
		*tag;
#ifdef WIN32
    HANDLE	 hso;
#else
    void	*hso;
#endif
    bp_slist	*list,
		*item;
    PROFILE_REGISTRATION
		*pr,
		*qr,
	       **rp,
		*(*beep_profile_init) (struct configobj *);

    sprintf (buffer, BEEP_LOAD_PROFILES, pgmname, dataname);
    if ((value = config_get (appconfig, buffer)) == NULL) {
        log_line (LOG_WRAP, 4, "no configuration data for %s", buffer);
        return NULL;
    }

    pr = NULL, rp = &pr;

    if ((list = bp_slist__split (value, ' ')) == NULL) {
        log_line (LOG_WRAP, 4, "empty configuration data for %s", buffer);
        return NULL;
    }

    size = 1;
    for (item = list; item; item = item -> next)
        size++;
    if (((libfiles = lib_malloc (sizeof *libfiles * size)) == NULL)
	    || ((initnames = lib_malloc (sizeof *initnames * size)) == NULL)) {
        log_line (LOG_WRAP, 5, "load_beep_profiles: out of memory");
        bp_slist__freeall (list);
        if (libfiles)
	    lib_free (libfiles);
	return NULL;
    }
    memset (libfiles, 0, sizeof *libfiles * size);
    memset (initnames, 0, sizeof *initnames * size);

    warnedP = 0;
    for (item = list; item; item = item -> next) {
        if ((tag = (char *) item -> data) == NULL)
	    continue;

	snprintf (buffer, sizeof buffer, BEEP_PRO_LIBFILE, tag);
	if ((value = config_get (appconfig, buffer)) == NULL) {
	    log_line (LOG_WRAP, 4, "no configuration data for %s", buffer);
	    continue;
	}
	for (pp = libfiles; *pp; pp++)
	    if (!strcmp (*pp, value))
		break;

	if (*pp != NULL)
	    log_line (LOG_WRAP, 1, "duplicate library %s, continuing...",
		      value);
	else {
	    *pp++ = value;

#ifdef WIN32
	    if ((hso = LoadLibrary (value)) == NULL) {
	        log_line (LOG_WRAP, 4, "error loading library %s (%d)\n",
			  value,
			  GetLastError ());
		continue;
	    }
#else
	    if ((hso = dlopen (value, RTLD_GLOBAL | RTLD_NOW)) == NULL) {
	        log_line (LOG_WRAP, 4, "%s: %s%s", dlerror (),
			  strerror (errno),
			  warnedP++ ? " -- check LD_LIBRARY_PATH" : "");
		continue;
	    }
#endif
	}
	
	snprintf (buffer, sizeof buffer, BEEP_PRO_INITNAME, tag);
	if ((value = config_get (appconfig, buffer)) == NULL) {
	    log_line (LOG_WRAP, 4, "no configuration data for %s", buffer);
	    continue;
	}
	for (pp = initnames; *pp; pp++)
	    if (!strcmp (*pp, value))
		break;

	if (*pp != NULL) {
	    log_line (LOG_WRAP, 1, "duplicate initname %s, continuing...",
		      value);
	    continue;
	}

	*pp++ = value;

#ifdef WIN32
	beep_profile_init = (PROFILE_REGISTRATION *(*) (struct configobj *))
						GetProcAddress (hso, value);
	if (beep_profile_init == NULL) {
	    log_line (LOG_WRAP, 4, "error finding entry point %s (%d)\n",
		      value,
		    GetLastError ());
	    if (NULL != hso) CloseHandle (hso);
	    continue;
	}
#else
	beep_profile_init = dlsym (hso, value);
	if ((tag = dlerror ()) != NULL) {
	    log_line (LOG_WRAP, 4, "%s: %s", tag, strerror (errno));
	    if (NULL != hso) dlclose (hso);
	    continue;
	}
#endif

	if ((*rp = beep_profile_init (appconfig)) == NULL) {
	    log_line (LOG_WRAP, 4, "%s returned NULL", value);
	    continue;
	}

	log_line (LOG_WRAP, 2, "loaded profile for %s", (*rp) -> uri);
	for (rp = &((*rp) -> next); (qr = *rp) != NULL; rp = &((*rp) -> next))
	    log_line (LOG_WRAP, 2, "loaded profile for %s", qr -> uri);
    }

    bp_slist__freeall (list);
    lib_free (libfiles);
    lib_free (initnames);

    return pr;
}
