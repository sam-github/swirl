/*
 * $Id: beepd.c,v 1.10 2002/03/28 14:09:35 huston Exp $
 *
 * generic beep listener 
 *
 * This is provided as an example of a beep listener which is perhaps suitable
 * for use for generic applications.  It is driven by the config file, in which
 * the profiles to be loaded are configured.
 *
 * The example provided, with the sample config file, should provide a test
 * program for the ECHO, TLS, and SASL profiles as provided with the library.
 */

/*    includes */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <syslog.h>
#include <unistd.h>
#else
#include <windows.h>
#include "win32/getopt.h"
#endif

#include "../utility/bp_config.h"
#include "../utility/logutil.h"
#include "../wrapper/bp_wrapper.h"
#include "../transport/bp_tcp.h"
#include "../wrapper/profile_loader.h"


/*    static variables */


static
char    *pgmname = "beepd";

static
char    *dataname = "default";

static
struct configobj *appconfig = NULL;


/*    main */

int
main (int        argc,
      char      *argv[]) {
    int                  i,
                         optchar;
    char                 buffer[BUFSIZ];
    char                *configF,
                        *hostName,
                         logF[BUFSIZ],
                        *portNo;
#ifndef WIN32
    DIAGNOSTIC          *d;
#endif
    PROFILE_REGISTRATION
                        *pr;

#ifndef WIN32
    if (!(pgmname = strrchr (argv[0], '/')) || !(*++pgmname))
        pgmname = argv[0];
#else
    pgmname = "beepd";
#endif

    /* initialize tcp-based wrapper library */

    tcp_bp_library_init ();


    /* create a configuration object */

    if (!(appconfig = config_new (NULL))) {
        fprintf (stderr, "config_new: failed\n");
        return 1;
    }

    configF = NULL, logF[0] = '\0', portNo = NULL;
    while ((optchar = getopt (argc, argv, "a:f:l:p:")) != -1)
        switch (optchar) {
            case 'a':
                if (!(dataname = optarg))
                    goto usage;
                break;
    
            case 'f':
                if (!optarg)
                    goto usage;
                if (configF) {
                    fprintf (stderr,
                             "specify \"-f configFile\" at most once\n");
                    goto usage;
                }
                if ((i = config_parse_file (appconfig, configF = optarg))
                        != CONFIG_OK) {
                    fprintf (stderr, "config_parse_file: failed %d\n", i);
                    return 1;
                }
                break;
    
            case 'l':
                if (!optarg)
                    goto usage;
                strcpy (logF, optarg);
                break;
    
            case 'p':
                if ((!(portNo = optarg)) || (atoi (portNo) <= 0))
                    goto usage;
                break;

            default:
                goto usage;
        }

    hostName = (argc == optind + 1) ? argv[optind++] : "";
    if (argc != optind) {
usage: ;

        fprintf (stderr,
                 "usage: %s [-a dataname] [-f configFile] [-l logFile]\n\
             [-p portNo]   [hostName]\n",
                 pgmname);
        return 1;
    }

    if (!portNo) {
        sprintf (buffer, "beep %s application %s port_number", pgmname, dataname);
        if (!(portNo = config_get (appconfig, buffer))) {
            fprintf (stderr, "specify \"-p portNo\"");
            if (configF)
                fprintf (stderr, " if \"-f %s\" doesn't define it", configF);
            else
                fprintf (stderr, " or use \"-f configFile\"");
            fprintf (stderr, "\n");
            goto usage;
        }
        if (atoi (portNo) <= 0) {
            fprintf (stderr, "invalid port_number in \"-f %s\": %s\n",
                     configF, portNo);
            goto usage;
        }
    }


    /* configure logging package, initialize the log */

#ifndef WIN32
    if ((d = log_config (appconfig, pgmname, dataname, logF)) != NULL) {
        fprintf (stderr, "unable to configure logging: [%d] %s\n",
                 d -> code, d -> message);
        bp_diagnostic_destroy (NULL, d);
        return 1;
    }
#endif


    /* dynamically load and initialize profiles */

    if (!(pr = load_beep_profiles (appconfig, pgmname, dataname))) {
        fprintf (stderr, "no profiles loaded, consult log file\n");
        return 1;
    }
    

    /* loop forever: accept connections */

    tcp_bp_listen (hostName, atoi (portNo), pr, appconfig);


    /* got a termination signal */

    tcp_bp_library_shutdown ();

    return 0;
}
