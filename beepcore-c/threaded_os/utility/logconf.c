/* includes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "logutil.h"
#include "../wrapper/bp_wrapper.h"

#ifndef	WIN32
#define SYSLOG_NAMES
#include <syslog.h>
#ifndef INTERNAL_MARK
#undef  SYSLOG_NAMES
#endif
#else
#define LOG_DAEMON (3<<3)
#define LOG_INFO 6
#endif


/*    defines and typedefs */


#define BEEP_LOG_MODE           "beep %s application %s log_mode"
#define BEEP_LOG_FACILITY       "beep %s application %s log_facility"
#define BEEP_LOG_NAME           "beep %s application %s log_name"
#define BEEP_LOG_IDENT          "beep %s application %s log_ident"
#define BEEP_LOG_SEVERITY       "beep %s application %s log_severity"


/*    static variables */


#ifdef  SYSLOG_NAMES
static
CODE modenames[] = {
    { "file",   LOG_MODE_FILE                   },
    { "syslog",                 LOG_MODE_SYSLOG },
    { "all",    LOG_MODE_FILE | LOG_MODE_SYSLOG },
    { "none",    0                              },
    { NULL,     -1                              }
};
#else
#define facilitynames   NULL
#define modenames       NULL
#define prioritynames   NULL
#endif


static int
log_config_int (struct configobj       *appconfig,
                char                   *pgmname,
                char                   *dataname,
                char                   *format,
#ifdef  SYSLOG_NAMES
                CODE                   *pairs,
#else
                void                   *dummy,
#endif
		int                     defval,
		int			severityP,
                struct diagnostic     **d);


/*    module entry point */


struct diagnostic *
log_config (struct configobj       *appconfig,
            char                   *pgmname,
            char                   *dataname,
            char                   *logname) {
    int          facility,
                 mode,
		 severity;
    char        *ident,
                *name;
    char         buffer[BUFSIZ];
    struct diagnostic
                *d;

    if (((mode = log_config_int (appconfig, pgmname, dataname,
                                 BEEP_LOG_MODE, modenames,
                                 LOG_MODE_FILE, 0, &d)) == -1)
        || ((facility = log_config_int (appconfig, pgmname, dataname,
                                        BEEP_LOG_FACILITY, facilitynames,
                                        LOG_DAEMON, 0, &d)) == -1)
        || ((severity = log_config_int (appconfig, pgmname, dataname,
                                        BEEP_LOG_SEVERITY, prioritynames,
                                        LOG_INFO, 1, &d)) == -1))
        return d;

    if (!logname[0]) {
        sprintf (buffer, BEEP_LOG_NAME, pgmname, dataname);
        if (!(name = config_get (appconfig, buffer)) || (!*name))
            sprintf (logname,
                     strcmp (dataname, "default") ? "%s-%s.log" : "%s.log",
                     pgmname, dataname);
        else
            strcpy (logname, name);
    }

    sprintf (buffer, BEEP_LOG_IDENT, pgmname, dataname);
    if (!(ident = config_get (appconfig, buffer)) || (!*ident))
        ident = pgmname;
    config_set (appconfig, BEEP_IDENT, ident);

    if (!log_create (mode, logname, ident, severity, facility))
        return NULL;

    return bp_diagnostic_new (NULL, 500, NULL, "%s: %s", logname,
                              strerror (errno));
}


static int
log_config_int (struct configobj       *appconfig,
                char                   *pgmname,
                char                   *dataname,
                char                   *format,
#ifdef  SYSLOG_NAMES
                CODE                   *pairs,
#else
                void                   *dummy,
#endif
                int                     defval,
                int                     severityP,
		struct diagnostic     **d) {
    int          value;
    char        *cp,
                *ep,
                 buffer[BUFSIZ];
#ifdef  SYSLOG_NAMES
    CODE	*c;
#endif

    sprintf (buffer, format, pgmname, dataname);
    if (!(cp = config_get (appconfig, buffer)) || (!*cp))
        return defval;

    value = strtol (cp, &ep, 0);
    if (!*ep)
        return value;

#ifdef  SYSLOG_NAMES
    for (c = pairs; c -> c_name; c++)
        if (!strcmp (c -> c_name, cp)) {
	    if (severityP)
	        return (LOG_DEBUG - c -> c_val);
            return c -> c_val;
	}
#endif

    if (!(ep = strrchr (format, ' ')) || !(*++ep))
        ep = format;

    *d = bp_diagnostic_new (NULL, 500, NULL,
			    "invalid %s field in configuration: %s", ep, cp);

    return (-1);
}
