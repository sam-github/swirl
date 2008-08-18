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
 * $Id: logutil.h,v 1.9 2002/01/01 00:25:15 mrose Exp $
 *
 * IW_log.h
 *
 */
/* This file defines the interface between the wrapper and the 
   logging facility. */
#ifndef _LOGUTIL_H
#define _LOGUTIL_H

#include "bp_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @name Logging functions
 */
//@{

/**
 * Explicitly initialize the logging system.
 *
 * Call this or {@link log_config log_config} before creating your first
 * {@link BP_CONNECTION connection} structure.
 * 
 * @param mode      Logging mode, any of:
 *		 <ul>
 *		 <li>LOG_MODE_FILE</li>
 *		 <li>LOG_MODE_SYSLOG</li>
 *		 </ul>
 *
 * @param filename File name to use, if <b>LOG_MODE_FILE</b> specified.
 *
 * @param ident     A string to be prepended to each log entry.
 *
 * @param severity  The lowest severity to be logged,
 *		    cf, {@link bp_log bp_log}.
 *
 * @param facility  A <i>syslog(3)</i> facility.
 *
 * @return 0 or <i>errno</i>.
 **/
extern int log_create(int mode,
		      char *filename,
		      char *ident,
		      int severity,
		      int facility);

/**
 * Implicitly initialize the logging system using a configuration object.
 *
 * Call this or {@link log_create log_create} before creating your first
 * {@link BP_CONNECTION connection} structure.
 * 
 * @param appconfig A configuration object which has the proper values set
 *                  defining the profiles to be loaded. For an example,
 *                  see <code>threaded_os/examples/beepd.cfg</code>
 *
 * @param pgmname Typically argv[0].
 *
 * @param dataname The application configuration, usually "default".
 *
 * @param logname The name of the logfile, or <b>NULL</b> to request a
 *		  suitable default.
 *
 * @return On success, <b>NULL</b>; otherwise, a pointer to a diagnostic
 *         structure.
 **/
extern struct diagnostic* log_config(struct configobj *appconfig,
				     char *pgmname,
				     char *dataname,
				     char *logname);
/**
 * Make a formatted log entry.
 *
 * Use {@link bp_log bp_log} instead of this function.
 * (Typically this function is is provided as an argument to
 * {@link bp_connection_create bp_connection_create} instead of being used
 * directly.)
 *
 * @param service The subsystem making the entry, one of:
 *		 <ul>
 *		 <li>LOG_MISC</li>
 *		 <li>LOG_CORE</li>
 *		 <li>LOG_WRAP</li>
 *		 <li>LOG_PROF</li>
 *		 </ul>
 *
 * @param severity The severity from <b>0</b> (lowest) to <b>7</b> (highest).
 *		   (Yes, this is inverted from the way <i>syslog</i> does it.)
 *
 * @param fmt... The format string and arguments.
 *
 **/
extern void log_line(int service, int severity, char * fmt, ...);

/* This function closes out the log file. */
/**
 * Explicitly terminate the logging system.
 *
 * Call this right before you exit.
 **/
void log_destroy(void);

#define LOG_MISC 0
#define LOG_CORE 1
#define LOG_WRAP 2
#define LOG_PROF 3

#define LOG_MODE_FILE         0x01
#define LOG_MODE_SYSLOG       0x02

#ifdef WIN32
#define LOG_USER (1<<3)
#endif


/**
 * The "appname" for the program.
 * <p>
 * Set by {@link log_config log_config}.
 **/
#define	BEEP_IDENT	"beep ident"

//@}

#ifdef __cplusplus
}
#endif

/* endif multiple-include protection */
#endif
