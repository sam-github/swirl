/*
 * $Id: beepng.c,v 1.33 2002/04/27 15:02:55 huston Exp $
 *
 * This is provided as an example of an initiator of a BEEP connection.
 *
 * The example provided, with the sample config file, should provide a test
 * program for the ECHO, TLS, and SASL profiles as provided with the library.
 */

/* generic beep initiator */

#ifndef WIN32
#define SYSLOG_RAW
#endif


/*    includes */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <syslog.h>
#endif
#ifdef LINUX
#include <sys/time.h>
#endif
#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#include "win32/getopt.h"
#include <crtdbg.h>
#endif

#include "../utility/bp_config.h"
#include "../utility/logutil.h"
#include "../wrapper/bp_wrapper.h"
#include "../transport/bp_tcp.h"
#include "../../profiles/null-profiles.h"
#ifndef CYRUS_SASL
#include "../../profiles/sasl-profiles.h"
#else
#include "../../profiles/cyrus-profiles.h"
#endif
#include "../../profiles/tls-profile.h"
#ifdef  SYSLOG_RAW
#include "../../profiles/syslog-raw.h"
#endif


#define DEFAULT_CONTENT_TYPE    "Content-Type: application/beep+xml\r\n\r\n"


/*    static variables */


static
char    *pgmname = "beepng";

static
char    *dataname = "default";

static
struct configobj *appconfig = NULL;


static int
beepng (BP_CONNECTION           *bp,
        PROFILE_REGISTRATION    *pr,
        int                      echoP,
        int                      count,
        int                      size);


#ifdef  SYSLOG_RAW
static  void    *syslog_v;
static  sem_t    syslog_sem;

static void
syslog_raw (BP_CONNECTION       *bp,
            int                  count,
            int                  size);

static void
callback_initiator (void        *v,
                    int          code,
                    char        *diagnostic,
                    void        *clientData);
#endif

/*  */

/*    main */

int
main (int        argc,
      char      *argv[]) {
    int                  i,
                         optchar;
    int                  count,
                         debugP,
                         size;
#ifdef  SYSLOG_RAW
    int                  syslogP;
#endif
    char                *ep,
                         buffer[BUFSIZ];
    char                 certF[BUFSIZ],
                        *configF,
                        *hostName,
                         keyF[BUFSIZ],
                         logF[BUFSIZ],
                        *mechanism,
                        *mode,
                         name1[BUFSIZ],
                         name2[BUFSIZ],
                         passPhrase[BUFSIZ],
                        *portNo;
    DIAGNOSTIC          *d;
    PROFILE_REGISTRATION
                        *pr,
                        *qr;
    BP_CONNECTION       *bp;

#ifndef WIN32
    if (!(pgmname = strrchr (argv[0], '/')) || !(*++pgmname))
        pgmname = argv[0];
#else
        pgmname = "beepng";
#endif


    /* initialize tcp-based wrapper library */

    tcp_bp_library_init ();


    /* create a configuration object */

    if (!(appconfig = config_new (NULL))) {
        fprintf (stderr, "config_new: failed\n");
        return 1;
    }

    configF = NULL, logF[0] = '\0';
    mode = "echo", count = 10, size = 10240;
#ifdef  SYSLOG_RAW
    syslogP = 0;
#endif
    certF[0] = '\0', keyF[0] = '\0';
    mechanism = "none", name1[0] = '\0', name2[0] = '\0', passPhrase[0] = '\0';
    debugP = 0;
    portNo = NULL;
    while ((optchar = getopt (argc, argv, "a:c:df:k:l:M:m:P:p:s:T:t:U:Z"))
               != -1)
        switch (optchar) {
            case 'a':
                if (!(dataname = optarg))
                    goto usage;
                break;
    
            case 'c':
                if (!optarg)
                    goto usage;
                if (((count = strtol (optarg, &ep, 10)) == 0) || (*ep))
                    goto usage;
                break;
    
            case 'd':
                debugP = 1;
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
    
            case 'k':
                if (!optarg)
                    goto usage;
                strcpy (keyF, optarg);
                if (!certF[0])
                    strcpy (certF, optarg);
                break;

            case 'l':
                if (!optarg)
                    goto usage;
                strcpy (logF, optarg);
                break;
    
            case 'M':
#ifndef CYRUS_SASL
                if (!(mechanism = optarg)
                        || (strcmp (mechanism, "anonymous")
                                && strcmp (mechanism, "otp")
                                && strcmp (mechanism, "none")))
                    goto usage;
#else
                if (!(mechanism = optarg))
                    goto usage;
#endif
                break;

            case 'm':
                if (!(mode = optarg)
                        || (strcmp (mode, "echo") && strcmp (mode, "sink")))
                    goto usage;
                break;
    
            case 'P':
                if (!optarg)
                    goto usage;
                strcpy (passPhrase, optarg);
                break;

            case 'p':
                if ((!optarg) || (atoi (portNo = optarg) <= 0))
                    goto usage;
                break;

            case 's':
                if (!optarg)
                    goto usage;
                if (((size = strtol (optarg, &ep, 10)) < 0) || (*ep))
                    goto usage;
                break;

            case 'T':
                if (!optarg)
                    goto usage;
                strcpy (name2, optarg);
                break;

            case 't':
                if (!optarg)
                    goto usage;
                strcpy (certF, optarg);
                if (!keyF[0])
                    strcpy (keyF, optarg);
                break;

            case 'U':
                if (!optarg)
                    goto usage;
                strcpy (name1, optarg);
                break;

#ifdef  SYSLOG_RAW
            case 'Z':
                syslogP = 1;
                break;
#endif

            default:
                goto usage;
        }

#ifndef CYRUS_SASL
    if ((!strcmp (mechanism, "anonymous")) && (!name2[0]))
        goto usage;
    if ((!strcmp (mechanism, "otp")) && ((!name1[0]) || (!passPhrase[0])))
        goto usage;
#endif

    if (argc != optind + 1) {
usage: ;

        fprintf (stderr,
#ifndef CYRUS_SASL
                 "usage: %s [-a dataName]      [-f configFile] [-l logFile]\n\
              [-m \"echo\"|\"sink\"] [-c count]      [-s size]\n\
              [-t certFile]      [-k keyFile]\n\
              [-M \"none\"]\n\
              [-M \"anonymous\"     -T traceInfo]\n\
              [-M \"otp\"           -U userName    [-T targetName] -P passPhrase]\n\
              [-d]\
              [-p portNo]            hostName\n",
#else
                 "usage: %s [-a dataName]      [-f configFile] [-l logFile]\n\
              [-m \"echo\"|\"sink\"] [-c count]      [-s size]\n\
              [-t certFile]      [-k keyFile]\n\
              [-M mechanism]     [-U userName]   [-T targetName] [-P passPhrase]\n\
              [-d]
              [-p portNo]            hostName\n",
#endif
                 pgmname);
        return 1;
    }
    hostName = argv[optind];

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

    if (debugP) {
        sprintf (buffer, "beep %s application %s log_severity", pgmname,
                 dataname);
        config_set (appconfig, buffer, "debug");
    }
    if ((d = log_config (appconfig, pgmname, dataname, logF)) != NULL) {
        fprintf (stderr, "unable to configure logging: [%d] %s\n",
                 d -> code, d -> message);
        bp_diagnostic_destroy (NULL, d);
        return 1;
    }


    /* initialize the profile we're going to use */

    switch (*mode) {
        case 'e':
            if (debugP)
                config_set (appconfig, NULL_ECHO_DEBUG, "1");
            pr = null_echo_Init (appconfig);
            break;

        case 's':
            if (debugP)
                config_set (appconfig, NULL_SINK_DEBUG, "1");
            pr = null_sink_Init (appconfig);
            break;

        default:
            pr = NULL;
            break;
    }
    if (!(qr = pr)) {
        printf ("unable to load profile for \"-m %s\"\n", mode);
        return 1;
    }
    while (qr -> next)
        qr = qr -> next;
    

    /* get the TLS profile, if need be... */

    if (certF[0]) {
        config_set (appconfig, TLS_CERTFILE, certF);
        config_set (appconfig, TLS_KEYFILE, keyF);
        if (debugP)
            config_set (appconfig, TLS_DEBUG, "1");

        if (!(qr -> next = tls_profile_Init (appconfig))) {
            printf ("unable to load TLS profile\n");
            return 1;
        }
        while (qr -> next)
            qr = qr -> next;
    }


    /* get the SASL profiles, if need be... */

    if (strcmp (mechanism, "none")) {
#ifdef  CYRUS_SASL
        int     result;

        cyrus_first ();
        if ((result = sasl_client_init (cyrus_client_callbacks (appconfig)))
                 != SASL_OK) {
            printf ("sasl_client_init failed (cyrus code %d) %s\n", result,
                    sasl_errstring (result, NULL, NULL));
            return 1;
        }
        if ((result = sasl_server_init (cyrus_server_callbacks (appconfig),
                                        pgmname))
                 != SASL_OK) {
            printf ("sasl_server_init failed (cyrus code %d) %s\n", result,
                    sasl_errstring (result, NULL, NULL));
            return 1;
        }
        if (strcmp (mechanism, "any"))
            config_set (appconfig, SASL_LOCAL_MECHANISM, mechanism);
        if ((name1[0] != '\0') && (name2[0] == '\0'))
            strcpy (name2, name1);
        else if ((name2[0] != '\0') && (name1[0] == '\0'))
          strcpy (name1, name2);
#else
        config_set (appconfig, SASL_LOCAL_MECHANISM, mechanism);
#endif
        if (name1[0] != '\0')
            config_set (appconfig, SASL_LOCAL_USERNAME, name1);
        if (name2[0] != '\0') {
#ifndef CYRUS_SASL
            config_set (appconfig, SASL_LOCAL_TRACEINFO, name2);
#endif
            config_set (appconfig, SASL_LOCAL_TARGET, name2);
        }
        if (passPhrase[0] != '\0')
            config_set (appconfig, SASL_LOCAL_PASSPHRASE, passPhrase);

#ifndef WIN32
#ifndef CYRUS_SASL
        if (debugP)
            config_set (appconfig, SASL_FAMILY_DEBUG, "1");

        if (!(qr -> next = sasl_profiles_Init (appconfig))) {
            printf ("unable to load SASL profiles\n");
            return 1;
        }
#else
        if (debugP)
            config_set (appconfig, SASL_CYRUS_DEBUG, "1");

        if (!(qr -> next = cyrus_profiles_Init (appconfig))) {
            printf ("unable to load Cyrus SASL profiles\n");
            return 1;
        }
#endif
#endif
        while (qr -> next)
            qr = qr -> next;
    }

#ifdef  SYSLOG_RAW
    if (syslogP) {
        if (debugP)
            config_set (appconfig, SYSLOG_RAW_DEBUG, "1");
        if (!(qr -> next = sr_Init (appconfig))) {
        printf ("unable to load SYSLOG reliable raw profile\n");
        return 1;
        }
        while (qr -> next)
            qr = qr -> next;
        SEM_INIT (&syslog_sem, 0);
        sr_initiator (qr, NULL, callback_initiator, NULL);
    }
#endif

    if (debugP)
        for (qr = pr; qr; qr = qr -> next)
            printf ("loaded profile for %s\n", qr -> uri);


    /* time for the real work */

    if ((d = tcp_bp_connect (hostName, atoi (portNo), pr, appconfig, &bp))
            != NULL)
        printf ("[%d] %s\n", d -> code, d -> message);
    else if ((d = bp_wait_for_greeting (bp)) != NULL)
        printf ("unable to establish session: [%d] %s\n", d -> code,
                d -> message);
    else {
        if (certF[0] && (d = tls_privatize (bp, hostName)))
            printf ("unable to privatize session: [%d] %s\n", d -> code,
                    d -> message);
#ifndef WIN32
        else if (strcmp (mechanism, "none")
#ifndef CYRUS_SASL
                     && (d = sasl_login (bp, hostName)))
#else
                     && (d = cyrus_login (bp, hostName, NULL, NULL, NULL,
                                          NULL)))
#endif
            printf ("unable to authenticate: [%d] %s\n", d -> code,
                    d -> message);
        else
#ifdef  SYSLOG_RAW
        if (syslogP)
            syslog_raw (bp, count, size);
        else
#endif
#endif
            (void) beepng (bp, pr, *mode == 'e', count, size);

        tcp_bp_connection_close (bp);
    }
    if (d)
        bp_diagnostic_destroy (bp, d);


    /* finalize tcp-based wrapper library */

    tcp_bp_library_shutdown ();


    /* de-allocate things we built that still remain... */

    log_destroy ();
    bp_profile_registration_chain_destroy (NULL, pr);
    config_destroy (appconfig);


    return 0;
}

/*  */

static int
beepng (BP_CONNECTION           *bp,
        PROFILE_REGISTRATION    *pr,
        int                      echoP,
        int                      count,
        int                      size) {
    int                  cc,
                         cnt,
                         len,
                         nlP,
                         status;
    unsigned long        usecs[3],
                        *avg = &usecs[1],
                        *max = &usecs[2],
                        *min = &usecs[0];
    char                 c,
                         prefix[BUFSIZ],
                        *b1,
                        *b2,
                        *cp,
                        *ep,
                        *sp;
    void                *v;

    if (!(v = null_start (bp, pr, NULL)))
        return 1;

    if (!count)
        count = 1;
    if (size < 0)
        size = 0;

    len = size + (sizeof DEFAULT_CONTENT_TYPE - 1)
            + (sizeof "<data></data>" - 1) + 1;
    if (!(b1 = lib_malloc (len)) || !(b2 = lib_malloc (len))) {
        if (b1)
            lib_free (b1);
        printf ("unable to allocate buffers\n");
        return 1;
    }

    sprintf (sp = b1, "%s<data>", DEFAULT_CONTENT_TYPE);
    sp += strlen (sp);
    cp = sp, ep = b1 + len - sizeof "</data>";

#ifndef WIN32
    srandom (1L);
#else
    srand (1L);
#endif
    while (cp < ep) {
        do {
#ifndef WIN32
          c = random () & 0x7f;
#else
          c = rand () & 0x7f;
#endif
        } while (!isalnum (c) && (c != ' '));
        *cp++ = c;
    }

    strcpy (cp, "</data>");
    len--;

    *min = *avg = *max = 0L;
    nlP = 0;
    status = 0;      
    for (cnt = 0; (count < 0)  || (cnt < count); cnt++) {
        unsigned long    clicks;
        struct timeval   tv1,
                         tv2;

#ifndef WIN32
        if (gettimeofday (&tv1, NULL) < 0) {
            perror ("gettimeofday");
            return 1;
        }
#else
        tv1.tv_sec = 0;
        tv1.tv_usec = 0;
#endif

        if ((cc = null_trip (v, b1, len, b2, len)) < 0) {
            printf ("null_trip: failed %d\n", cc);
            status = 1;
            break;
        }

#ifndef WIN32
        if (gettimeofday (&tv2, NULL) < 0) {
            perror ("gettimeofday");
            return 1;
        }
#else
        tv2.tv_sec = 0;
        tv2.tv_usec = 0;
#endif
        clicks = (tv2.tv_sec - tv1.tv_sec) * 1000000
                        + (tv2.tv_usec - tv1.tv_usec);
        if (*max <= 0)
            *min = *max = clicks;
        else {
            if (clicks < *min)
                *min = clicks;
            if (clicks > *max)
                *max = clicks;
        }
        *avg += clicks;

        if (echoP && ((cc != len) || memcmp (b1, b2, len))) {
            if (nlP)
                printf ("\n"), nlP = 0;
            printf ("%s mismatch\n", cc != len ? "length" : "data");
            fflush (stdout);
            status = 1;
            memcpy (b2, b1, len);
        } else
            printf (".");
        if (nlP++ >= 78) {
            printf ("\n");
            fflush (stdout);
            nlP = 0;
        }

        if (size > 1) {
            memcpy (sp + 1, b2 + (sp - b1), ep - (sp + 1));
            *sp = b2[(ep - b1) - 1];
        }
    }
    if (nlP)
        printf ("\n");

    lib_free (b1);
    lib_free (b2);

    sprintf (prefix, "%d round-trips: min/avg/max =", cnt);
    printf ("%s %lu %lu %lu (usecs)\n", prefix, *min, *avg = *avg/cnt, *max);
    if (size > 0) {
        unsigned long   *up,
                        *vp;

        len = strlen (prefix);
        printf ("%*.*s", len, len, "");

        for (vp = (up = usecs) + (sizeof usecs/sizeof usecs[0]);
                 up < vp;
                 up++)
            if (*up > 0) {
                int     speed = (int) ((size * 1000000.0) / *up) >> 10;
#ifndef WIN32
                len = log10 (*up) + 1;
#else
                len = (int)log10 (*up) + 1;
#endif
                printf (" %*.*d", len, len, speed);
            } else
                printf ("  ");

        printf (" (KB/s)\n");
    }

    if ((cc != NULL_DONE) && (null_close (v) < 0)) {
        printf ("null_close: failed %d\n", cc);
        status = 1;
    }

    return status;
}

/*  */

#ifdef  SYSLOG_RAW
#define ENTRY1  "<29>Oct 27 13:21:08 ductwork imxpd[141]: Heating emergency."
#define ENTRY2  "<29>Oct 27 13:22:15 ductwork imxpd[141]: Contact Tuttle."

static void
syslog_raw (BP_CONNECTION       *bp,
            int                  count,
            int                  size) {
    int     cnt,
            s;

    if (!count)
        count = 1;

    bp_log (bp, LOG_MISC, 0, "waiting for start callback");
    SEM_WAIT (&syslog_sem);
    if (syslog_v == NULL)
        return;

    s = SR_OK;
    for (cnt = 0; (count < 0)  || (cnt < count); cnt++) {
        fprintf (stderr, "entry %d\n", cnt + 1);

        if ((s = sr_log (syslog_v, (cnt % 2) ? ENTRY1 : ENTRY2)) != SR_OK) {
            fprintf (stderr, "sr_log: failed %d\n", s);
            break;
        }
    }

    switch (s) {
        case SR_OK:
        case SR_ERROR:
            if ((s = sr_fin (syslog_v)) != SR_OK)
                fprintf (stderr, "sr_fin: failed %d\n", s);
            else {
                bp_log (bp, LOG_MISC, 0, "waiting for final callback");
                SEM_WAIT (&syslog_sem);
            }
            break;

        default:
            break;
    }
}


static void
callback_initiator (void        *v,
                    int          code,
                    char        *diagnostic,
                    void        *clientData) {
    fprintf (stderr,
             "sr callback_initiator v=0x%x code=%d diagnostic=\"%s\"\n",
             (unsigned int) v, code, diagnostic);
    fflush (stderr);

    log_line (LOG_MISC, 0, "got callback code=%d", code);
    syslog_v = v;
    SEM_POST (&syslog_sem);
}
#endif
