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
 * $Id: logutil.c,v 1.10 2002/03/27 08:53:40 huston Exp $
 *
 * IW_log.c
 *
 * Basic logging functionality.
 */

#include "logutil.h"
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#ifndef WIN32
#include <syslog.h>
#else
#define vsnprintf _vsnprintf
#endif

static int	log_mode = 0;
static int	log_severity = 0;
#ifndef	WIN32
static int	log_syslogP = 0;
#endif
static char	log_ident[8 + 1];
static FILE    *log_file = NULL;

static char *log_categories[] = { "misc", "core", "wrap", "prof" };


void flog_line (int   category,
		int   severity,
		char *diagnostic) {
    time_t now;
    struct tm *tm;

    if ((log_file == NULL) || (severity < log_severity))
        return;

    if ((category < 0)
	    || (category > sizeof log_categories / sizeof log_categories[0]))
	category = 0;

    now = time (NULL);
    tm = localtime (&now);
    fprintf  (log_file, "%02d/%02d %02d:%02d:%02d %s %d.%-4s %s\n",
	      tm -> tm_mon + 1, tm -> tm_mday, tm -> tm_hour, tm -> tm_min,
	      tm -> tm_sec, log_ident, severity, log_categories[category],
	      diagnostic);
    fflush  (log_file);
}

#ifndef WIN32
void slog_line (int   category,
		int   severity,
		char *diagnostic) {
    if (!log_syslogP || (severity < log_severity))
	return;

    if ((category < 0)
	    || (category > sizeof log_categories / sizeof log_categories[0]))
	category = 0;

    syslog (LOG_DEBUG - severity, "%s %s", log_categories[category],
	    diagnostic);
}
#endif

int log_create (int   mode,
		char *filename,
		char *ident,
		int   severity,
		int   facility) {
    int	    width = sizeof log_ident - 1;

    log_mode = mode;
    log_severity = severity;
    sprintf (log_ident, "%-*.*s", width, width, ident);

    if (LOG_MODE_FILE & log_mode) {
        if (filename == NULL) {
	    log_file = stderr;
	    return 0;
	}

	if ((log_file = fopen (filename, "a")) == NULL)
	    return errno;
    }

#ifndef WIN32
    if (LOG_MODE_SYSLOG & log_mode) {
        openlog (ident, LOG_PID | LOG_NDELAY, facility);
	log_syslogP = 1;
    }
#endif

    log_line (LOG_CORE, 2, "start logging");

    return 0;
}

void log_line (int   category,
	       int   severity,
	       char *fmt, ...) {
    char    buffer[BUFSIZ];
    va_list ap;

    va_start (ap, fmt);
    vsnprintf (buffer, sizeof buffer, fmt, ap);
    va_end (ap);

    if (LOG_MODE_FILE & log_mode)
        flog_line (category, severity, buffer);

#ifndef WIN32
    if (LOG_MODE_SYSLOG & log_mode)
        slog_line (category, severity, buffer);
#endif
}

void log_destroy (void) {
    if ((log_file != NULL) && (log_file != stderr)) {
	flog_line (LOG_CORE, 2, "done logging");
        fclose (log_file), log_file = NULL;
    }

#ifndef WIN32
    if (log_syslogP)
        closelog (), log_syslogP = 0;
#endif
}
