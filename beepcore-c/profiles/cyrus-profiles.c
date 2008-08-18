/* profiles for CYRUS SASL (requires v2.1.0 or later) */

/* NB:

        please don't mess with the '#undef tuning', that part is still
        under development...

 */

/*
 * $Id: cyrus-profiles.c,v 1.9 2002/01/20 23:03:40 mrose Exp $
 */


/*    includes */

#ifndef WIN32
#include <ctype.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#else
#include <windows.h>
#define sem_t HANDLE
#endif
#include "cyrus-profiles.h"
#include "sasl/saslutil.h"
#include "../threaded_os/utility/logutil.h"

/*    defines and typedefs */

#undef  tuning

#define SASL_MIN_SSF               0
#define SASL_MAX_SSF             256
#define SASL_MAX_BUFSIZ         4096

#define PRO_PREFIX_URI          "http://iana.org/beep/SASL/"

#define DEFAULT_CONTENT_TYPE    "Content-Type: application/beep+xml\r\n\r\n"


typedef struct pro_globaldata {
    int                  pl_flags;
#define PRO_DEBUG       (1<<0)

    char                *pl_mechanism;
    char                 pl_uri[sizeof PRO_PREFIX_URI + SASL_MECHNAMEMAX];

    sasl_conn_callback_t pl_callback;
    void                *pl_clientData;
}  PRO_GLOBALDATA;


typedef struct pro_localdata {
    int                  pl_flags;
#define PRO_INITIATOR   (1<<1)
#define PRO_STARTING    (1<<2)
#define PRO_ABORTING    (1<<3)
#define PRO_COMPLETED   (1<<4)
#define PRO_CLOSED      (1<<5)
#define PRO_SKIPSTEP    (1<<6)

    CHANNEL_INSTANCE    *pl_channel;
    DIAGNOSTIC          *pl_status;
    sasl_interact_callback_t
                         pl_interact;
    void                *pl_clientData;

    sasl_conn_t         *pl_conn;
}  PRO_LOCALDATA;


/*    statics */

static sem_t    global_sem;

static BP_CONNECTION *global_bp;
static char *global_mechlist;


static char    *pro_name (char *uri);

static int      sasl_make_conn (BP_CONNECTION  *bp,
                                PRO_GLOBALDATA *pl,
                                sasl_conn_t   **conn,
                                char           *serverName,
                                DIAGNOSTIC    **d);

static char    *sasl_response (PROFILE_INSTANCE *pi,
                               int               result,
                               const char       *optr,
                               unsigned          olen,
                               int               piggyP);

static void     sasl_next (PROFILE_INSTANCE *pi,
                           char             *data,
                           int               size);             
                           
static int      sasl_make_xyz (BP_CONNECTION *bp,
                               int            result);

static char    *sasl_make_blob (PROFILE_INSTANCE *pi,
                                int               scode,
                                const char       *optr,
                                unsigned          olen,
                                int               piggyP);
#define STATUS_ABORT            100
#define STATUS_COMPLETE         101
#define STATUS_CONTINUE         SASL_CONTINUE

static int      sasl_parse_blob (PROFILE_INSTANCE *pi,
                                 const char       *iptr,
                                 unsigned          ilen,
                                 const char      **optr,
                                 unsigned         *olen,
                                 int              *scode);
#define SASL_NOBLOB             (-100)
#define SASL_BADBLOB            (-101)

static void     sasl_parse_error (PROFILE_INSTANCE *pi,
                                  char             *data,
                                  int               size);

static void     sasl_set_local_success (PROFILE_INSTANCE *pi);
static void     sasl_set_local_failure (PROFILE_INSTANCE *pi,
                                        DIAGNOSTIC       *d);
static void     sasl_set_remote_success (PROFILE_INSTANCE *pi);
static void     sasl_set_remote_failure (PROFILE_INSTANCE *pi,
                                         int               result,
                                         int               code,
                                         const char       *reason);

static void     sasl_tuning_complete (PROFILE_INSTANCE *pi,
                                      int               successP);

static int      sasl_reader (void  *handle,
                             char  *inbuf,
                             char **outbuf,
                             int    inlen,
                             int   *inused);
static int      sasl_writer (void  *handle,
                             char  *inbuf,
                             char **outbuf,
                             int    inlen,
                             int   *inused);
static int      sasl_destroy (void *handle);

static int      sasl_log (void       *context,
                          int         level,
                          const char *message);
static int      sasl_path (void        *context,
                           const char **path);


/*    profile methods */

static char *
pro_name (char *uri) {
    char        *cp;

    if ((cp = strrchr (uri, '/')) != NULL)
        return ++cp;
    return uri;
}

static char *
pro_connection_init (PROFILE_REGISTRATION *pr,
                     BP_CONNECTION        *bp) {
    if (((PRO_GLOBALDATA *) pr -> user_ptr) -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s connection_init\n", pro_name (pr -> uri));
        fflush (stderr);
    }

    return NULL;
}

static char *
pro_session_init (PROFILE_REGISTRATION *pr,
                  BP_CONNECTION        *bp) {
    int             result;
    char            buffer[SASL_MECHNAMEMAX + sizeof "  "];
    PRO_GLOBALDATA *pl = (PRO_GLOBALDATA *) pr -> user_ptr;
    sasl_conn_t    *conn = NULL;

    if (pl -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s session_init: role='%c' mode=\"%s\"\n",
                 pro_name (pr -> uri), bp -> role, bp -> mode);
        fflush (stderr);
    }

    if (bp -> role == INST_ROLE_INITIATOR)
        return NULL;

    SEM_WAIT (&global_sem);

    if (global_bp != bp) {
        global_bp = bp;
        if (global_mechlist)
            lib_free (global_mechlist), global_mechlist = NULL;
    }

    result = SASL_OK;
    if (!global_mechlist) {
        unsigned    size;
        const char *mechanisms;

        if ((result = sasl_make_conn (bp, pl, &conn, NULL, NULL)) != SASL_OK)
            goto out;

        if ((result = sasl_listmech (conn, NULL, " ", " ", " ", &mechanisms,
                                     &size, NULL)) != SASL_OK) {
            bp_log (bp, LOG_PROF, 6, "sasl_listmech failed (cyrus code %d) %s",
                    result, sasl_errstring (result, NULL, NULL));
            goto out;
        }

        if (!(global_mechlist = lib_malloc (size + 1))) {
            bp_log (bp, LOG_PROF, 5, "%s session_init: out of memory",
                    pro_name (pr -> uri));
            goto out;             
        }
        strcpy (global_mechlist, mechanisms);
    }

    sprintf (buffer, " %s ", pl -> pl_mechanism);
    if (!strstr (global_mechlist, buffer)) {
        bp_log (bp, LOG_PROF, 1, "%s not in list %s", pl -> pl_mechanism,
                global_mechlist);
        result = SASL_NOMECH;
    }

out: ;
    SEM_POST (&global_sem);

    if (conn)
        sasl_dispose (&conn);
    return ((result == SASL_OK) ? NULL : "");
}

static char *
pro_session_fin (PROFILE_REGISTRATION *pr,
                 BP_CONNECTION        *bp) {
    if (((PRO_GLOBALDATA *) pr -> user_ptr) -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s session_fin\n", pro_name (pr -> uri));
        fflush (stderr);
    }

    return NULL;
}

static char *
pro_connection_fin (PROFILE_REGISTRATION *pr,
                    BP_CONNECTION        *bp) {
    if (((PRO_GLOBALDATA *) pr -> user_ptr) -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s connection_fin\n", pro_name (pr -> uri));
        fflush (stderr);
    }

    return NULL;
}

static void
pro_start_indication (PROFILE_INSTANCE *pi,
                      PROFILE          *po) {
    int                  result,
                         status;
    unsigned             ilen,
                         olen;
    char                *cp;
    const char          *iptr,
                        *optr;
    DIAGNOSTIC           ds,
                        *d = &ds;
    PRO_GLOBALDATA      *pl = (PRO_GLOBALDATA *) pi -> channel
                                                    -> profile_registration
                                                    -> user_ptr;
    PRO_LOCALDATA       *il;
    PROFILE              ps,
                        *p = &ps;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);
    struct cfgsearchobj *cs;

    if (pl -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s start_indication: piggyback=\"%s\"\n",
                 pro_name (po -> uri),
                 po -> piggyback ? po -> piggyback : "<NULL>");
        fflush (stderr);
    }

    memset (p, 0, sizeof *p);
    p -> uri = po -> uri;

    memset (d, 0, sizeof *d);
    d -> code = 421;

    if (((cp = config_get (appconfig, SASL_LOCAL_CODE)) != NULL)
            && (*cp == '2')) {
        d -> code = 520;
        d -> message = "already tuned for authentication";

        bpc_start_response (pi -> channel, p, d);
        return;
    }

    if (!(il = (PRO_LOCALDATA *) lib_malloc (sizeof *il))) {
        d -> message = "out of memory";

        bpc_start_response (pi -> channel, p, d);
        return;
    }
    pi -> user_ptr1 = il;

    memset (il, 0, sizeof *il);
    il -> pl_flags = pl -> pl_flags;

    if ((result = sasl_make_conn (pi -> channel -> conn, pl, &il -> pl_conn,
                                  NULL, NULL)) != SASL_OK)
        goto oops;

    optr = NULL, olen = 0;
    if (((result = sasl_parse_blob (pi, po -> piggyback,
                                    po -> piggyback_length, &iptr, &ilen,
                                    &status)) == SASL_OK)
            && (status != STATUS_ABORT))
        result = sasl_server_start (il -> pl_conn, pl -> pl_mechanism,
                                    iptr, ilen, &optr, &olen);
    if (iptr)
        lib_free ((void *) iptr);

    switch (result) {
        case SASL_OK:
            if (status == STATUS_ABORT)
                result = STATUS_ABORT;
        /* and fall... */

        default:
            il -> pl_flags |= PRO_COMPLETED;
            break;

        case SASL_CONTINUE:
            break;
    }

    cs = config_search_init (appconfig, SASL_REMOTE_PREFIX, "");
    for (cp = config_search_string_firstkey (cs);
             cp;
             cp = config_search_string_nextkey (cs))
        config_delete (appconfig, cp);
    config_search_fin (&cs);
    config_set (appconfig, SASL_REMOTE_MECHANISM, pl -> pl_mechanism);

    if (!(iptr =  sasl_response (pi, result, optr, olen, 1)))
        goto oops;

#ifdef  tuning
    bpc_tuning_reset_request (pi -> channel);
#endif

    p -> piggyback_length = strlen (p -> piggyback = (char *) iptr);

    bpc_start_response (pi -> channel, p, NULL);
    if (result == SASL_OK)
        sasl_set_remote_success (pi);
    lib_free ((void *) iptr);
    return;

oops: ;
    pi -> user_ptr1 = NULL;

    if (il -> pl_conn)
        sasl_dispose (&il -> pl_conn);
    lib_free (il);
    d -> message = "out of memory";

    bpc_start_response (pi -> channel, p, d);
}

static void
pro_start_confirmation (void             *clientData,
                        PROFILE_INSTANCE *pi,
                        PROFILE          *po) {
    PRO_GLOBALDATA      *pl = (PRO_GLOBALDATA *) pi -> channel
                                                    -> profile_registration
                                                    -> user_ptr;
    PRO_LOCALDATA       *il = clientData;

    if (pl -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s start_confirmation: piggyback=\"%s\"\n",
                 pro_name (po -> uri),
                 po -> piggyback ? po -> piggyback : "<NULL>");
        fflush (stderr);
    }

    pi -> user_ptr1 = il;

    il -> pl_flags |= pl -> pl_flags;
    il -> pl_flags &= ~(PRO_STARTING | PRO_CLOSED);
    il -> pl_channel = pi -> channel;

#ifdef  tuning
    bpc_tuning_reset_request (pi -> channel);
#endif

    sasl_next (pi, po -> piggyback, po -> piggyback_length);
}

static void
pro_close_indication (PROFILE_INSTANCE *pi,
                      DIAGNOSTIC       *request,
                      char              origin,
                      char              scope) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr,
                 "%s close_indication: request=[%d] \"%s\" origin=%c scope=%c\n",
                 pro_name (pi -> channel -> profile_registration -> uri),
                 request -> code, request -> message ? request -> message
                                                     : "<NULL>",
                 origin, scope);
        fflush (stderr);
    }
               
    bpc_close_response (pi -> channel, NULL);
}

static void
pro_close_confirmation (PROFILE_INSTANCE *pi,
                        char              status,
                        DIAGNOSTIC       *error,
                        char              origin,
                        char              scope) {
    PRO_LOCALDATA *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (il -> pl_flags & PRO_DEBUG) {
        if (error)
            fprintf (stderr,
                     "%s close_confirmation: status=%c error=[%d] \"%s\" origin=%c scope=%c\n",
                     pro_name (pi -> channel -> profile_registration -> uri),
                     status, error -> code, error -> message ? error -> message
                                                             : "<NULL>",
                     origin, scope);
        else 
            fprintf (stderr,
                     "%s close_confirmation: status=%c no error origin=%c scope=%c\n",
                     pro_name (pi -> channel -> profile_registration -> uri),
                     status, origin, scope);
        fflush (stderr);
    }

    if (status != PRO_ACTION_SUCCESS)
        return;

    if (il) {
        if (il -> pl_flags & PRO_INITIATOR) {
            il -> pl_flags |= PRO_CLOSED;
        } else {
            pi -> user_ptr1 = NULL;

            if (il -> pl_conn)
                sasl_dispose (&il -> pl_conn);
            lib_free (il);
        }
    }
}

static void
pro_tuning_reset_indication (PROFILE_INSTANCE *pi) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s tuning_reset_indication\n",
                 pro_name (pi -> channel -> profile_registration -> uri));
        fflush (stderr);
    }
}

static void
pro_tuning_reset_confirmation (PROFILE_INSTANCE *pi,
                               char              status) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s tuning_reset_confirmation: status=%c\n",
                 pro_name (pi -> channel -> profile_registration -> uri),
                 status);
        fflush (stderr);
    }

    pro_close_confirmation (pi, status, NULL, PRO_ACTION_LOCAL,
                             PRO_ACTION_SESSION);
}

static void
pro_message_available (PROFILE_INSTANCE *pi) {
    int                  result,
                         size,
                         status;
    unsigned             ilen,
                         olen;
    char                *buffer,
                        *payload;
    const char          *iptr,
                        *optr;
    FRAME               *f;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (!(f = bpc_query_message (pi -> channel, BLU_QUERY_ANYTYPE,
                                 BLU_QUERY_ANYMSG, BLU_QUERY_ANYANS)))
        return;

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr,
                 "%s message_available: type=%c number=%ld answer=%ld size=%ld\n",
                 pro_name (pi -> channel -> profile_registration -> uri),
                 f -> msg_type, f -> message_number, f -> answer_number,
                 f -> size);
        fprintf (stderr, f -> size > 150 ? "%-150.150s...\n" : "%s\n",
                 f -> payload);
        fflush (stderr);
    }

    if (il -> pl_flags & PRO_ABORTING) {
        il -> pl_flags |= PRO_COMPLETED;
        goto drop;
    }

    size = bpc_frame_aggregate (pi -> channel, f, &buffer);
    if (buffer == NULL) {
        bp_log (pi -> channel -> conn, LOG_PROF, 5,
                "%s message_available: out of memory aggregating message",
                 pro_name (pi -> channel -> profile_registration -> uri));
        il -> pl_flags |= PRO_COMPLETED;
        goto drop;
    }

    if (il -> pl_flags & PRO_COMPLETED)
        payload = bpc_error_allocate (pi -> channel, 521, NULL,
                                      "already done");
    else if (il -> pl_flags & PRO_INITIATOR) {
        sasl_next (pi, buffer, size);
        goto out;
    } else {
        optr = NULL, olen = 0;
        if (((result = sasl_parse_blob (pi, buffer, size, &iptr, &ilen,
                                        &status)) == SASL_OK)
                && (status != STATUS_ABORT))
            result = sasl_server_step (il -> pl_conn, iptr, ilen,
                                       &optr, &olen);
        if (iptr)
              lib_free ((void *) iptr);

        switch (result) {
            case SASL_OK:
                if (status == STATUS_ABORT)
                    result = STATUS_ABORT;
            /* and fall... */

            default:
                il -> pl_flags |= PRO_COMPLETED;
                break;

            case SASL_CONTINUE:
                break;
        }

        payload = sasl_response (pi, result, optr, olen, 0);
    }

    if (payload)
        bpc_send (pi -> channel,
                  (strstr (payload, "<error") != NULL)
                      ? BLU_FRAME_TYPE_ERR : BLU_FRAME_TYPE_RPY,
                  f -> message_number, f -> answer_number,
                  BLU_FRAME_COMPLETE, payload, strlen (payload));
    else {
        bp_log (pi -> channel -> conn, LOG_PROF, 5,
                "%s message: out of memory sending message",
                pro_name (pi -> channel -> profile_registration -> uri));
        il -> pl_flags |= PRO_COMPLETED;
    }
    if (result == SASL_OK)
        sasl_set_remote_success (pi);

out: ;
    bpc_buffer_destroy (pi -> channel, buffer);

drop: ;
    bpc_frame_destroy (pi -> channel, f);
}

static void
pro_window_full (PROFILE_INSTANCE *pi) {
    DIAGNOSTIC    *d;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s window_full\n",
                 pro_name (pi -> channel -> profile_registration -> uri));
        fflush (stderr);
    }

    bp_log (pi -> channel -> conn, LOG_PROF, 5, "SASL channel window full!?!");
    if (!(d = bpc_close_request (pi -> channel, BLU_CHAN0_MSGNO_DEFAULT, 550,
                                 NULL, "stop misbehaving", NULL, NULL))) {
        bp_log (pi -> channel -> conn, LOG_PROF, 4,
                "unable to close SASL channel: [%d] %s", d -> code,
                d -> message);

        bp_diagnostic_destroy (pi -> channel -> conn, d);
    }    
}


/*    helper functions */

static int
sasl_make_conn (BP_CONNECTION  *bp,
                PRO_GLOBALDATA *pl,
                sasl_conn_t  **conn,
                char          *serverName,
                DIAGNOSTIC   **d) {
    int                         result;
    char                       *cp;
    struct configobj           *appconfig = bp_get_config (bp);
    sasl_security_properties_t  sps,
                               *sp = &sps;

    if (!*conn) {
        if (!(cp = config_get (appconfig, SASL_CYRUS_SERVICE)))
            cp = "beep";
        if ((result = (pl -> pl_callback != NULL)
                          ? (*pl -> pl_callback) (bp, conn,
                                                  pl -> pl_clientData,
                                                  (d == NULL) ? 1 : 0)
                          : (d != NULL)
                                ? sasl_client_new (cp, serverName, NULL, NULL,
                                                   NULL, SASL_SUCCESS_DATA,
                                                   conn)
                                : sasl_server_new (cp, NULL, NULL, NULL, NULL,
                                                   NULL, SASL_SUCCESS_DATA,
                                                   conn))
                != SASL_OK) {
            bp_log (bp, LOG_PROF, 6, "sasl_%s_new failed (cyrus code %d) %s",
                    (d != NULL) ? "client" : "server" , result,
                    sasl_errstring (result, NULL, NULL));
            goto out;
        }
    }
    
    if (pl -> pl_callback != NULL) {
        const void    *pvalue;

        if ((result = sasl_getprop (*conn, SASL_SEC_PROPS, &pvalue))
                != SASL_OK) {
            bp_log (bp, LOG_PROF, 6,
                    "sasl_getprop of SASL_SEC_PROPS failed (cyrus code %d) %s",
                    result, sasl_errstring (result, NULL, NULL));
            goto out;
        }
        memcpy (sp, pvalue, sizeof *sp);
    }
    else {
        memset (sp, 0, sizeof *sp);

        if ((cp = config_get (appconfig, SASL_CYRUS_MINSSF)) != NULL)
            sp -> min_ssf = atoi (cp);
        else
            sp -> max_ssf = SASL_MIN_SSF;

        if ((cp = config_get (appconfig, SASL_CYRUS_MAXSSF)) != NULL)
            sp -> max_ssf = atoi (cp);
        else
            sp -> max_ssf = SASL_MAX_SSF;

        sp -> maxbufsize = SASL_MAX_BUFSIZ;
    }

    if ((cp = config_get (appconfig, PRIVACY_REMOTE_SUBJECTNAME)) != NULL
            && ((result = sasl_setprop (*conn, SASL_AUTH_EXTERNAL, cp))
                    != SASL_OK)) {
        bp_log (bp, LOG_PROF, 6,
                "sasl_setprop of AUTH_EXTERNAL to '%s' failed (cyrus code %d) %s",
                cp, result, sasl_errstring (result, NULL, NULL));
        goto out;
    }

    if ((cp = config_get (appconfig, PRIVACY_CIPHER_SBITS)) != NULL) {
        sp -> max_ssf = atoi (cp);

        if ((result = sasl_setprop (*conn, SASL_SSF_EXTERNAL, &sp -> max_ssf))
                != SASL_OK) {
            bp_log (bp, LOG_PROF, 6,
                    "sasl_setprop of SSF_EXTERNAL to '%s' failed (cyrus code %d) %s",
                    cp, result, sasl_errstring (result, NULL, NULL));
            goto out;
        }
    }
#ifndef tuning
    else
        sp -> min_ssf = sp -> max_ssf = 0;
#endif

    if ((result = sasl_setprop (*conn, SASL_SEC_PROPS, sp)) != SASL_OK) {
        bp_log (bp, LOG_PROF, 6,
                "sasl_setprop of SASL_SEC_PROPS failed (cyrus code %d) %s",
                result, sasl_errstring (result, NULL, NULL));
        goto out;
    }

    if (d)
        *d = NULL;

    return SASL_OK;

out: ;
    if (d)
        *d = bp_diagnostic_new (bp, 450, NULL,
                                (char *) sasl_errstring (result, NULL, NULL));
    
    if (*conn)
        sasl_dispose (conn), *conn = NULL;

    return result;
}

static char *
sasl_response (PROFILE_INSTANCE *pi,
               int               result,
               const char       *optr, 
               unsigned          olen,
               int               piggyP) {
    int          code;
    char        *cp,
                *ep;
    const char  *diagnostic,
                *language;

    switch (result) {
        case SASL_CONTINUE:
            return sasl_make_blob (pi, STATUS_CONTINUE, optr, olen, piggyP);

        case SASL_OK:
            return sasl_make_blob (pi, STATUS_COMPLETE, optr, olen, piggyP);

        case STATUS_ABORT:
            sasl_set_remote_failure (pi, result, 550, "Aborted by client");
            return sasl_make_blob (pi, STATUS_COMPLETE, optr, olen, piggyP);

        case SASL_BADBLOB:
            sasl_set_remote_failure (pi, result, 501, "Invalid blob format");
            return bpc_error_allocate (piggyP ? NULL : pi -> channel, 501,
                                       NULL, "Invalid blob format");

        default:
            sasl_set_remote_failure (pi, result,
                                     code = sasl_make_xyz (pi -> channel
                                                              -> conn, result),
                                     sasl_errstring (result, NULL, NULL));
            if (code == 535)
                result = SASL_BADAUTH;
            if ((cp = bp_server_localize (pi -> channel -> conn)) != NULL) {
                for (ep = cp; (ep = strchr (ep, ' ')) != NULL; *ep++ = ',')
                    continue;
                diagnostic = sasl_errstring (result, cp, &language);
                for (ep = cp; (ep = strchr (ep, ',')) != NULL; *ep++ = ' ')
                    continue;
            }
            else
                diagnostic = sasl_errstring (result, NULL, &language);
            return bpc_error_allocate (piggyP ? NULL : pi -> channel, code,
                                       (char *) language, (char *) diagnostic);
    }
}

static void
sasl_next (PROFILE_INSTANCE *pi,
           char             *data,
           int               size) {
    int            code,
                   result,
                   status;
    unsigned       ilen,
                   olen;
    char          *payload;
    const char    *iptr,
                  *optr,
                  *reason;
    DIAGNOSTIC     ds,
                  *d = &ds,
                  *d2;
    PRO_LOCALDATA *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (strstr (data, "<error") != NULL) {
        sasl_parse_error (pi, data, size);
        return;
    }

    memset (d, 0, sizeof *d);

    iptr = NULL;
    optr = NULL, olen = 0;
    if (size == 0)
        result = SASL_NOBLOB, code = 501, reason = "expecting blob element";
    else if ((result = sasl_parse_blob (pi, data, size, &iptr, &ilen, &status))
                 != SASL_OK)
        code = 501, reason = "unable to parse blob element";
    else if ((il -> pl_flags & PRO_SKIPSTEP)) {
        if (status != STATUS_COMPLETE) {
            code = 521, reason = "server says continue, client says complete";
            result = SASL_BADBLOB;
        }
    } else {
        sasl_interact_t *interact = NULL;

        while ((result = sasl_client_step (il -> pl_conn, iptr, ilen,
                                           &interact, &optr, &olen))
                   == SASL_INTERACT) {
            if (status == STATUS_COMPLETE)
                goto lost_sync;
            if ((d2 = (*il -> pl_interact) (pi -> channel -> conn, interact,
                                            il -> pl_clientData)) != NULL) {
                result = SASL_BADBLOB;
                code = d2 -> code, reason = d2 -> message;
                bp_diagnostic_destroy (pi -> channel -> conn, d2);
            }
        }
        if ((result == SASL_CONTINUE) && (status == STATUS_COMPLETE)) {
lost_sync: ;
            code = 521, reason = "server says complete, client says continue";
            result = SASL_BADBLOB;
        }       
    }
    if (iptr)
        lib_free ((void *) iptr);

    switch (result) {
        default:
            code = sasl_make_xyz (pi -> channel -> conn, result);
            reason = sasl_errstring (result, NULL, NULL);
        /* and fall... */

        case SASL_NOBLOB:
        case SASL_BADBLOB:
            il -> pl_flags |= PRO_ABORTING;
            d -> code = code, d -> message = (char *) reason;
            sasl_set_local_failure (pi, d);
            payload = sasl_make_blob (pi, status = STATUS_ABORT, NULL, 0, 0);
            break;

        case SASL_OK:
            if (status == STATUS_COMPLETE) {
                il -> pl_flags |= PRO_COMPLETED;
                break;
            }
            il -> pl_flags |= PRO_SKIPSTEP;
        /* else fall... */

        case SASL_CONTINUE:
            payload = sasl_make_blob (pi, status = STATUS_CONTINUE, optr, olen,
                                      0);
            break;
    }
    
    switch (status) {
        case STATUS_CONTINUE:
        case STATUS_ABORT:
        default:
            if (payload)
                bpc_send (pi -> channel, BLU_FRAME_TYPE_MSG,
                          BLU_FRAME_MSGNO_UNUSED, BLU_FRAME_IGNORE_ANSNO,
                          BLU_FRAME_COMPLETE, payload, strlen (payload));
            else {
                bp_log (pi -> channel -> conn, LOG_PROF, 5,
                        "%s sasl_next: out of memory sending message",
                        pro_name (pi -> channel -> profile_registration
                                     -> uri));
                il -> pl_flags |= PRO_COMPLETED;
            }
            break;

        case STATUS_COMPLETE:
            sasl_set_local_success (pi);
            break;
    }
}

static int
sasl_make_xyz (BP_CONNECTION *bp,
               int result) {
    int     code;

    switch (result) {
        default:
            bp_log (bp, LOG_PROF, 6, "unknown cyrus code %d", result);
            /* and fall... */

        case SASL_BADPARAM:
        case SASL_NOTINIT:
            code = 500;
            break;

        case SASL_FAIL:
        case SASL_NOMECH:
        case SASL_BADPROT:
        case SASL_NOTDONE:
            code = 550;
            break;

        case SASL_BADMAC:
        case SASL_BADAUTH:
        case SASL_NOAUTHZ:
        case SASL_TOOWEAK:
        case SASL_ENCRYPT:
        case SASL_TRANS:
        case SASL_EXPIRED:
        case SASL_DISABLED:
        case SASL_NOUSER:
        case SASL_BADVERS:
        case SASL_UNAVAIL:
        case SASL_NOVERIFY:
            code = 535;
            break;

        case SASL_TRYAGAIN:
            code = 430;
            break;

        case SASL_NOMEM:
        case SASL_BUFOVER:
            code = 450;
            break;
    }

    return code;
}

static char *
sasl_make_blob (PROFILE_INSTANCE *pi,
                int               scode,
                const char       *optr,
                unsigned          olen,
                int               piggyP) {
    int          result,
                 size;
    char        *buffer,
                *ep,
                *status;

    switch (scode) {
        case STATUS_CONTINUE:
        default:
            status = NULL;
            break;

        case STATUS_ABORT:
            status = "abort";
            break;

        case STATUS_COMPLETE:
            status = "complete";
            break;
    }

    size = (sizeof DEFAULT_CONTENT_TYPE - 1)
                + (sizeof "<blob status=''></blob>" - 1)
                + ((status != NULL) ? strlen (status) : 0)
                + (((olen + 2) * 4) / 3) + 1;
    if (!(buffer = piggyP ? lib_malloc (size)
                          : bpc_buffer_allocate (pi -> channel, size)))
        return NULL;
    ep = buffer;

    if (!piggyP) {
        strcpy (ep, DEFAULT_CONTENT_TYPE);
        ep += strlen (ep);
    }

    if (status != NULL)
        sprintf (ep, "<blob status='%s'", status);
    else
        strcpy (ep, "<blob");
    ep += strlen (ep);

    if (olen > 0) {
        *ep++ = '>';
        if ((result = sasl_encode64 (optr, olen, ep,
                                     size - ((ep - buffer) + sizeof "</blob>"),
                                     &olen)) != SASL_OK) {
            if (piggyP)
                lib_free (buffer);
            else
                bpc_buffer_destroy (pi -> channel, buffer);
            if (pi)
                bp_log (pi -> channel -> conn, LOG_PROF, 6,
                        "got cyrus code %d on sasl_encode64", result);
            else
                log_line (LOG_PROF, 6,
                        "got cyrus code %d on sasl_encode64", result);

            return NULL;
        }
        ep += olen;
        strcpy (ep, "</blob>");
    } else
        strcpy (ep, " />");

    return buffer;
}

static int
sasl_parse_blob (PROFILE_INSTANCE *pi,
                 const char       *iptr,
                 unsigned          ilen,
                 const char      **optr,
                 unsigned         *olen,
                 int              *scode) {
    int      result;
    char    *bp,
            *cp,
            *dp,
            *ep;

    *optr = NULL, *olen = 0;
    *scode = STATUS_CONTINUE;

    if (ilen == 0)
        return SASL_OK;

    if (!(bp = strstr (iptr, "<blob"))
            || !(cp = strchr (bp, '>')))
        return SASL_BADBLOB;
    *cp++ = '\0';

    if ((ep = strstr (bp, "status=")) != NULL) {
        char    *fp;

        ep += sizeof "status=" - 1;
        if (!(fp = strchr (ep + 1, *ep)))
            return SASL_BADBLOB;
        *fp = '\0';
        ep++;

        if (!strcmp (ep, "abort"))
            *scode = STATUS_ABORT;
        else if (!strcmp (ep, "complete"))
            *scode = STATUS_COMPLETE;
    }

    if (((dp = strchr (cp, '<')) != NULL) && (dp > cp)) {
        int     size = dp - cp;

        if (!(*optr = lib_malloc (size)))
            return SASL_NOMEM;
        if ((result = sasl_decode64 (cp, size, (char *) *optr, size, olen))
                != SASL_OK) {
            bp_log (pi -> channel -> conn, LOG_PROF, 6,
                    "got cyrus code %d on sasl_decode64", result);

            lib_free ((void *) *optr), *optr = NULL;
            return result;
        }       
    }
    
    return SASL_OK;
}

static void
sasl_parse_error (PROFILE_INSTANCE *pi,
                  char             *data,
                  int               size) {
    char            *cp;
    DIAGNOSTIC       ds,
                    *d = &ds;
    FRAME           *f;
        
    if ((cp = strstr (data, "<error")) != NULL)
        size -= cp - data, data = cp;

    memset (d, 0, sizeof *d);
    if (!(f = bpc_frame_allocate (pi -> channel, size))) {
        d -> code = 421;
        d -> message = "out of memory decoding SASL error";
    } else {
        f -> msg_type = BLU_FRAME_TYPE_ERR;
        strcpy (f -> payload, data);

        if (!(d = bpc_parse_error (pi -> channel, f))) {
            d -> code = 500;
            d -> message = "unable to parse error message from server";
        }
    }
            
    sasl_set_local_failure (pi, d);

    if (d != &ds)
        bp_diagnostic_destroy (pi -> channel -> conn, d);
    if (f)
        bpc_frame_destroy (pi -> channel, f);
}

static void
sasl_set_local_success (PROFILE_INSTANCE *pi) {
    int                  code;
    char                *reason;
    BP_CONNECTION       *bp = pi -> channel -> conn;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    struct configobj    *appconfig = bp_get_config (bp);

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "sasl_set_local_success\n");
        fflush (stderr);
    }

    if (config_test_and_set (appconfig, SASL_LOCAL_CODE, "200") != CONFIG_OK) {
        code = atoi (config_get (appconfig, SASL_LOCAL_CODE));
        reason = config_get (appconfig, SASL_LOCAL_REASON);
    } else {
        code = 200;
        config_set (appconfig, SASL_LOCAL_REASON, reason = "login successful");

        sasl_tuning_complete (pi, 1);
    }

    il -> pl_status = bp_diagnostic_new (bp, code, NULL, "%s", reason);
}

static void
sasl_set_local_failure (PROFILE_INSTANCE *pi,
                        DIAGNOSTIC       *d) {
    int                  code;
    char                 buffer[BUFSIZ],
                        *reason;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    code = d -> code;
    reason = d -> message ? d -> message : "";

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "sasl_set_local_failure: code=%d reason=\"%s\"\n",
                 code, reason);
        fflush (stderr);
    }

    if (!il -> pl_status) {
        sprintf (buffer, "%d", code);
        config_set (appconfig, SASL_LOCAL_CODE, buffer);
        config_set (appconfig, SASL_LOCAL_REASON, reason);

        sasl_tuning_complete (pi, 0);

        il -> pl_status = bp_diagnostic_new (pi -> channel -> conn, code, NULL,
                                             "%s", reason);
    }
}

static void
sasl_set_remote_success (PROFILE_INSTANCE *pi) {
    const void       *pvalue;
    int               result;
    char             *cp;
    BP_CONNECTION    *bp = pi -> channel -> conn;
    PRO_GLOBALDATA   *pl = (PRO_GLOBALDATA *) pi -> channel
                                                 -> profile_registration
                                                 -> user_ptr;
    PRO_LOCALDATA    *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    struct configobj *appconfig = bp_get_config (bp);

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "sasl_set_remote_success\n");
        fflush (stderr);
    }

    if (config_test_and_set (appconfig, SASL_REMOTE_CODE, "200")
            == CONFIG_OK) {
        config_set (appconfig, SASL_REMOTE_REASON, "login successful");

        if ((result = sasl_getprop (il -> pl_conn, SASL_USERNAME, &pvalue))
                != SASL_OK) {
            bp_log (bp, LOG_PROF, 6,
                    "unable to determine SASL_USERNAME (cyrus code %d) %s",
                    result, sasl_errstring (result, NULL, NULL));
            cp = "";
        } else {
            config_set (appconfig, SASL_REMOTE_TARGET, cp = (char *) pvalue);
        }
        
        bp_log (bp, LOG_PROF, 2, "sasl 200 %s %s", pl -> pl_mechanism, cp);

        sasl_tuning_complete (pi, 1);
    }
}

static void
sasl_set_remote_failure (PROFILE_INSTANCE *pi,
                         int               result,
                         int               code,
                         const char       *reason) {
    char        buffer[BUFSIZ];
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr,
                 "sasl_set_remote_failure: result=%d code=%d reason=\"%s\"\n",
                 result, code, reason);
        fflush (stderr);
    }

    sprintf (buffer, "%d", code);
    if (config_test_and_set (appconfig, SASL_REMOTE_CODE, buffer)
            == CONFIG_OK) {
        config_set (appconfig, SASL_REMOTE_REASON, (char *) reason);

        bp_log (pi -> channel -> conn, LOG_PROF, 1, "cyrus code %d", result);
        bp_log (pi -> channel -> conn, LOG_PROF, 3, "sasl %d %s", code,
                (char *) reason);

        sasl_tuning_complete (pi, 0);
    }
}

static void
sasl_tuning_complete (PROFILE_INSTANCE *pi,
                      int               successP) {
    int               result;
    char              buffer[BUFSIZ];
    BP_CONNECTION    *bp = pi -> channel -> conn;
    PRO_LOCALDATA    *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    struct configobj *appconfig = bp_get_config (bp);
    sasl_ssf_t       *ssf;

    if (sasl_getprop (il -> pl_conn, SASL_SSF, (const void **) &ssf)
            != SASL_OK) {
        bp_log (bp, LOG_PROF, 6,
                "unable to determine SASL_SSF (cyrus code %d) %s",
                result, sasl_errstring (result, NULL, NULL));
        return;
    }
    if (*ssf <= 0) {
        if (il -> pl_flags & PRO_DEBUG) {
            fprintf (stderr,
                     "negotiation of a security layer wasn't attempted\n");
            fflush (stderr);
        }

#ifdef  tuning
        bpc_tuning_reset_complete (pi -> channel, PRO_ACTION_FAILURE,
                                   NULL);
#endif
        return;
    }

    if (!successP) {
        if (il -> pl_flags & PRO_DEBUG) {
            fprintf (stderr, "negotiation of a security layer failed\n");
            fflush (stderr);
        }

#ifdef  tuning
        bpc_tuning_reset_complete (pi -> channel, PRO_ACTION_SUCCESS,
                                   "plaintext");
#endif
        return;
    }

    switch (*ssf) {
        case 0:
            return;

        case 1:
            break;

        default:
#if 0
            config_set (appconfig, PRIVACY_CIPHER_NAME, ...);
            config_set (appconfig, PRIVACY_CIPHER_PROTOCOL, ...);
#endif
            sprintf (buffer, "%d", *ssf);
            config_set (appconfig, PRIVACY_CIPHER_SBITS, buffer);
            break;
    }

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr,
                 "negotiation of a security layer succeeded, mode is %s\n",
                 *ssf > 1 ? "encrypted" : "plaintext");
        fflush (stderr);
    }

    bp_push_rwfilter (bp, sasl_reader, il -> pl_conn, sasl_writer,
                      il -> pl_conn, sasl_destroy, NULL);
#ifdef  tuning
    bpc_tuning_reset_complete (pi -> channel, PRO_ACTION_SUCCESS,
                               *ssf > 1 ? "encrypted" : "plaintext");
#endif
}


static int
sasl_reader (void  *handle,
             char  *inbuf,
             char **outbuf,
             int    inlen,
             int   *inused) {
    int          result;
    unsigned     olen;
    const char  *optr;
    sasl_conn_t *conn = (sasl_conn_t *) handle;

fprintf(stderr,"sasl_reader %d octets\n",inlen);
fflush(stderr);

    if ((result = sasl_decode (conn, inbuf, inlen, &optr, &olen)) != SASL_OK) {
        log_line (LOG_PROF, 6, "sasl_decode failed (cyrus code %d) %s",
                  result, sasl_errstring (result, NULL, NULL));
        return (-1);
    }

fprintf(stderr,"    %u octets\n    [%*.*s]\n",olen,(int)olen,(int)olen,optr);
fflush(stderr);

    if (!(*outbuf = lib_malloc (olen))) {
        log_line (LOG_PROF, 6, "sasl_reader: out of memory");
        return (-1);
    }
    memcpy (*outbuf, optr, olen);
    *inused = inlen;
    
    return olen;

}

static int
sasl_writer (void  *handle,
             char  *inbuf,
             char **outbuf,
             int    inlen,
             int   *inused) {
    int          result;
    unsigned     olen;
    const char  *optr;
    sasl_conn_t *conn = (sasl_conn_t *) handle;

fprintf(stderr,"sasl_writer %d octets\n    [%*.*s]\n",inlen,inlen,inlen,inbuf);
fflush(stderr);

    if ((result = sasl_encode (conn, inbuf, inlen, &optr, &olen)) != SASL_OK) {
        log_line (LOG_PROF, 6, "sasl_encode failed (cyrus code %d) %s",
                  result, sasl_errstring (result, NULL, NULL));
        return (-1);
    }

fprintf(stderr,"    %u octets\n    -> [%*.*s]\n",olen,(int)olen,(int)olen,optr);
fflush(stderr);

    if (!(*outbuf = lib_malloc (olen))) {
        log_line (LOG_PROF, 6, "sasl_writer: out of memory");
        return (-1);
    }
    memcpy (*outbuf, optr, olen);
    *inused = inlen;
    
    return olen;
}

static int
sasl_destroy (void *handle) {
    sasl_conn_t *conn = (sasl_conn_t *) handle;

    if (conn)
        sasl_dispose (&conn);

    return 0;
}

static int
sasl_log (void       *context,
          int         level,
          const char *message) {
    BP_CONNECTION *bp = (BP_CONNECTION *) context;

    level = 7 - level;
    if (bp)
        bp_log (bp, LOG_PROF, level, "%s", message);
    else
        log_line (LOG_PROF, level, "%s", message);

    return SASL_OK;
}

static int
sasl_path (void        *context,
           const char **path) {
    struct configobj *appconfig = (struct configobj *) context;

    if (!path)
        return SASL_BADPARAM;

    *path = config_get (appconfig, SASL_CYRUS_PLUGINPATH);

    return SASL_OK;
}


/*
 * Additional profile-related functions.
 */

sasl_callback_t *
cyrus_client_callbacks (struct configobj *appconfig) {
    char                  *cp;
    sasl_callback_t       *cb;
    static sasl_callback_t callbacks[7];

    memset (cb = callbacks, 0, sizeof callbacks);

    cb -> id = SASL_CB_GETREALM;
    cb++;

    cb -> id = SASL_CB_USER;
    cb++;

    cb -> id = SASL_CB_AUTHNAME;
    cb++;

    cb -> id = SASL_CB_PASS;
    cb++;

    cb -> id = SASL_CB_LOG;
    cb -> proc = sasl_log;
    cb++;

    if ((cp = config_get (appconfig, SASL_CYRUS_PLUGINPATH)) != NULL) {
        cb -> id = SASL_CB_GETPATH;
        cb -> context = appconfig;
        cb -> proc = sasl_path;
        cb++;
    }

    cb -> id = SASL_CB_LIST_END;

    return callbacks;
}

sasl_callback_t *
cyrus_server_callbacks (struct configobj *appconfig) {
    char                  *cp;
    sasl_callback_t       *cb;
    static sasl_callback_t callbacks[3];

    memset (cb = callbacks, 0, sizeof callbacks);

    cb -> id = SASL_CB_LOG;
    cb -> proc = sasl_log;
    cb++;

    if ((cp = config_get (appconfig, SASL_CYRUS_PLUGINPATH)) != NULL) {
        cb -> id = SASL_CB_GETPATH;
        cb -> context = appconfig;
        cb -> proc = sasl_path;
        cb++;
    }
    
    cb -> id = SASL_CB_LIST_END;

    return callbacks;
}

int
cyrus_callback_setlog (BP_CONNECTION   *bp,
                       sasl_callback_t *cb) {                 
    for (; cb -> id != SASL_CB_LIST_END; cb++)
        if (cb -> id == SASL_CB_LOG) {
            cb -> proc = sasl_log;
            cb -> context = bp;
            return 0;
        }

    return (-1);
}

struct diagnostic *
cyrus_fillin (BP_CONNECTION   *bp,
              sasl_interact_t *interact,
              void            *clientData) {
#define SASL_RESPONSE_CNT          4
#define SASL_RESPONSE_MAX       1024

    char             *cp,
                     *dp;
    struct configobj *appconfig = bp_get_config (bp);
    static int        offset = 0;
    static char       response[SASL_RESPONSE_MAX * SASL_RESPONSE_CNT];

    for (; interact -> id != SASL_CB_LIST_END; interact++) {
        dp = response + (offset * SASL_RESPONSE_MAX);

        switch (interact -> id) {
            case SASL_CB_GETREALM:
                cp = SASL_LOCAL_REALM;
                goto check_config;

            case SASL_CB_USER:
                cp = SASL_LOCAL_TARGET;
                goto check_config;

            case SASL_CB_AUTHNAME:
                cp = SASL_LOCAL_USERNAME;
                goto check_config;

            case SASL_CB_PASS:
                cp = SASL_LOCAL_PASSPHRASE;
            /* and fall... */

check_config: ;
                if ((cp = config_get (appconfig, cp)) != NULL) {
                    interact -> len = strlen (interact -> result = cp);
                    continue;
                }
                if (interact -> id == SASL_CB_PASS) {
                    char    *cp = getpass (interact -> prompt);

                    strcpy (dp, cp);
                    memset (cp, '\0', interact -> len = strlen (dp));
                    break;
                }
            /* else fall... */

            default:
                fprintf (stderr, "%s:", interact -> prompt);
                fflush (stderr);
                fgets (dp, SASL_RESPONSE_MAX - 1, stdin);
                if ((interact -> len = strlen (dp)) > 0)
                    *(dp + --interact -> len) = '\0';
                break;
        }

        interact -> result = dp;
        offset = ++offset % SASL_RESPONSE_CNT;
    }

    return NULL;
}

void
cyrus_tune (PROFILE_REGISTRATION *pr,
            sasl_conn_callback_t  callback,
            void                 *clientData,
            int                   allP) {
    PRO_GLOBALDATA      *pl = (PRO_GLOBALDATA *) pr -> user_ptr;

    pl -> pl_callback = callback;
    pl -> pl_clientData = clientData;

    if (allP && ((pr -> next) != NULL))
        cyrus_tune (pr -> next, callback, clientData, allP);
}

DIAGNOSTIC *
cyrus_login (BP_CONNECTION             *bp,
             char                      *serverName,
             sasl_conn_callback_t       callback1,
             void                      *clientData1,
             sasl_interact_callback_t   callback2,
             void                      *clientData2) {
    int                  olen,
                         result;
    char                 buffer[sizeof PRO_PREFIX_URI + SASL_MECHNAMEMAX],
                        *cp,
                       **ip = NULL,
                        *iptr = NULL,
                        *mechanism,
                        *mechanisms = NULL,
                       **op,
                       **pp;
    const char          *mechused,
                        *optr;
    DIAGNOSTIC          *d;
    PROFILE              ps,
                        *p = &ps;
    PRO_GLOBALDATA       pls,
                        *pl = &pls;
    PRO_LOCALDATA       *il;
    sasl_interact_t     *interact;
    struct configobj    *appconfig = bp_get_config (bp);

    if (((cp = config_get (appconfig, SASL_REMOTE_CODE)) != NULL)
            && (*cp == '2'))
        return bp_diagnostic_new (bp, 520, NULL,
                                  "already tuned for authentication");

    if (!(il = (PRO_LOCALDATA *) lib_malloc (sizeof *il)))
        return bp_diagnostic_new (bp, 421, NULL, "out of memory");

    memset (il, 0, sizeof *il);
    il -> pl_flags = PRO_INITIATOR | PRO_STARTING | PRO_CLOSED;
    il -> pl_interact = (callback2 != NULL) ? callback2 : cyrus_fillin;
    il -> pl_clientData = clientData2;

    memset (pl, 0, sizeof *pl);
    pl -> pl_callback = callback1;
    pl -> pl_clientData = clientData1;

    if ((result = sasl_make_conn (bp, pl, &il -> pl_conn, serverName, &d))
            != SASL_OK)
        goto out;    

    if (!(ip = bp_profiles_avail_initiate (bp))) {
        d = bp_diagnostic_new (bp, 550, NULL,
                               "no profiles available to initiate");
        goto out;
    }
    if (!(op = bp_profiles_offered (bp))) {
        d = bp_diagnostic_new (bp, 550, NULL, "no profiles offered");
        goto out;
    }

    if ((mechanism = config_get (appconfig, SASL_LOCAL_MECHANISM)) != NULL) {
        strcpy (cp = buffer, PRO_PREFIX_URI);
        for (cp += strlen (cp); *mechanism; mechanism++)
            *cp++ = islower (*mechanism) ? toupper (*mechanism) : *mechanism;
        *cp = '\0';
        mechanism = buffer + sizeof PRO_PREFIX_URI - 1;

        for (pp = ip; *pp; pp++)
            if (!strcmp (*pp, buffer))
                break;
        if (!*pp) {
            d = bp_diagnostic_new (bp, 550, NULL,
                                   "mechanism '%s' not available", mechanism);
            goto out;
        }

        for (pp = op; *pp; pp++)
            if (!strcmp (*pp, buffer))
                break;
        if (!*pp) {
            d = bp_diagnostic_new (bp, 550, NULL, "mechanism '%s' not offered",
                                   mechanism);
            goto out;
        }
    }
    else {
        int       size = 0;
        char    **qp;

        for (pp = ip; *pp; pp++)
            if (!strncmp (*pp, PRO_PREFIX_URI, sizeof PRO_PREFIX_URI - 1))
                for (qp = op; *qp; qp++)
                    if (!strcmp (*qp, *pp)) {
                        size += strlen (*pp) + 3 - sizeof PRO_PREFIX_URI;
                        break;
                    }
        if (size == 0) {
            d = bp_diagnostic_new (bp, 550, NULL, "no mechanisms available");
            goto out;
        }

        if (!(mechanisms = lib_malloc (size + 1))) {
            d = bp_diagnostic_new (bp, 421, NULL, "out of memory");
            goto out;
        }
        cp = mechanisms;

        for (pp = ip; *pp; pp++)
            if (!strncmp (*pp, PRO_PREFIX_URI, sizeof PRO_PREFIX_URI - 1))
                for (qp = op; *qp; qp++)
                    if (!strcmp (*qp, *pp)) {
                        sprintf (cp, "%s ", *pp + (sizeof PRO_PREFIX_URI - 1));
                        cp += strlen (cp);
                        break;
                    }
        *--cp = '\0';

        mechanism = mechanisms; 
    }

    interact = NULL;
    while ((result = sasl_client_start (il -> pl_conn, mechanism, &interact,
                                        &optr, &olen, &mechused))
                == SASL_INTERACT) {
        if ((d = (*il -> pl_interact) (bp, interact, il -> pl_clientData))
                != NULL)
            goto out;
    }

    switch (result) {
        case SASL_OK:
            il -> pl_flags |= PRO_SKIPSTEP;
        /* and fall... */

        case SASL_CONTINUE:
            break;

        default:
            d = bp_diagnostic_new (bp, 450, NULL,
                                   (char *) sasl_errstring (result, NULL,
                                                            NULL));
            goto out;
    }

    memset (p, 0, sizeof *p);
    sprintf (p -> uri = buffer, "%s%s", PRO_PREFIX_URI, mechused);
    if (!(iptr = sasl_make_blob (NULL, STATUS_CONTINUE, optr, olen, 1))) {
        d = bp_diagnostic_new (bp, 421, NULL, "out of memory");
        goto out;
    }
    p -> piggyback_length = strlen (p -> piggyback = iptr);

    config_delete (appconfig, SASL_LOCAL_CODE);
    config_delete (appconfig, SASL_LOCAL_REASON);
    config_set (appconfig, SASL_LOCAL_MECHANISM, (char *) mechused);

    if ((d = bp_start_request (bp, BLU_CHAN0_CHANO_DEFAULT,
                               BLU_CHAN0_MSGNO_DEFAULT, p, serverName, NULL,
                               (void *) il)) != NULL)
        goto out;

    while ((il -> pl_status == NULL)
               && ((il -> pl_flags & (PRO_STARTING | PRO_CLOSED))
                        != PRO_CLOSED)
               && (bp -> status != INST_STATUS_EXIT))
        YIELD ();

    if (!il -> pl_status) {
        d =  bp_diagnostic_new (bp, 450, NULL,
                                bp -> status != INST_STATUS_EXIT
                                    ? "lost channel during tuning"
                                    : "lost session during tuning");
        goto out;
    }

    if (il -> pl_status -> code >= 400)
        d = il -> pl_status;
    else
        bp_diagnostic_destroy (NULL, il -> pl_status);

out: ;
    if (ip)
        lib_free (ip);
    if (iptr)
        lib_free (iptr);
    if (mechanisms)
        lib_free (mechanisms);

    if (!(il -> pl_flags & PRO_CLOSED))
        il -> pl_channel -> profile -> user_ptr1 = NULL;

    if (il -> pl_conn)
        sasl_dispose (&il -> pl_conn);
    lib_free (il);

    return d;
}


/*
 * Well-known shared library entry point.
 */

PROFILE_REGISTRATION *cyrus_profiles_Init (struct configobj *appconfig) {
    int                   debugP;
    char                 *cp;
    const char          **dp;
    PROFILE_REGISTRATION  prs,
                         *pr = &prs,
                         *rr = NULL,
                        **rrp = &rr;

    debugP = 0;
    if ((cp = config_get (appconfig, SASL_CYRUS_DEBUG)) != NULL)
        debugP = atoi (cp);

    memset (pr, 0, sizeof *pr);

    pr -> full_messages = 1;
    pr -> thread_per_channel = 0;

    pr -> proreg_connection_init = pro_connection_init;
    pr -> proreg_connection_fin  = pro_connection_fin;
    pr -> proreg_session_init = pro_session_init;
    pr -> proreg_session_fin  = pro_session_fin;
    pr -> proreg_start_indication = pro_start_indication;
    pr -> proreg_start_confirmation = pro_start_confirmation;
    pr -> proreg_close_indication   = pro_close_indication;
    pr -> proreg_close_confirmation = pro_close_confirmation;
    pr -> proreg_tuning_reset_indication   = pro_tuning_reset_indication;
    pr -> proreg_tuning_reset_confirmation = pro_tuning_reset_confirmation;
    pr -> proreg_message_available = pro_message_available;
    pr -> proreg_window_full       = pro_window_full;

    if (!(dp = sasl_global_listmech ())) {
        log_line (LOG_PROF, 6, "sasl_global_listmech failed\n");
        return NULL;
    }

    for (; *dp; dp++) {
        PRO_GLOBALDATA       *pl;
        PROFILE_REGISTRATION *qr;

        for (qr = rr; qr; qr = qr -> next)
            if (!strcmp (qr -> uri + sizeof PRO_PREFIX_URI - 1, *dp))
                break;

        if (!qr) {
            if (!(qr = (PROFILE_REGISTRATION *) lib_malloc (sizeof *qr +
                                                            sizeof *pl))) {
                log_line (LOG_PROF, 6, "unable to allocate PR");
                goto oops;
            }
            memcpy (qr, pr, sizeof *pr);
            *rrp = qr, rrp = &qr -> next;

            qr -> user_ptr = pl = (PRO_GLOBALDATA *) (((char *) qr)
                                                            + sizeof *qr);
            memset (pl, 0, sizeof *pl);
            if (debugP)
                pl -> pl_flags |= PRO_DEBUG;
            sprintf (qr -> uri = pl -> pl_uri, "%s%s", PRO_PREFIX_URI, *dp);
            pl -> pl_mechanism = pl -> pl_uri + sizeof PRO_PREFIX_URI - 1;

            qr -> initiator_modes = qr -> listener_modes =
                    "plaintext,encrypted";
        }
    }
    if (!rr)
        log_line (LOG_PROF, 6, "no mechanisms?!?");

    return rr;

oops: ;
    if (rr != NULL)
        bp_profile_registration_chain_destroy (NULL, rr);

    return NULL;
}


/*
 * Cyrus library glue
 */

struct realloc_hack {
    unsigned long size;
    char     data[1];
};

static void *
sasl_malloc (unsigned long size) {
    struct realloc_hack *r;

    if (!(r = lib_malloc (sizeof *r + size)))
        return NULL;
    r -> size = size;

    return (((void *) r) + sizeof *r);
}

static void
sasl_free (void *ptr) {
    if (ptr)
        lib_free (ptr - sizeof (struct realloc_hack));
}

static void *
sasl_calloc (unsigned long number,
             unsigned long size) {
    void *v;

    size *= number;
    if ((v = sasl_malloc (size)) != NULL)
        memset (v, 0, size);

    return v;
}

static void *
sasl_realloc (void          *ptr,
              unsigned long  size) {
    struct realloc_hack *r;
    void    *v;

    if (!ptr)
        return sasl_malloc (size);

    r = (struct realloc_hack *) (ptr - sizeof *r);
    if (r -> size <= size)
        return ptr;

    if ((v = sasl_malloc (size)) != NULL)
        memcpy (v, ptr, r -> size), sasl_free (ptr);
    return v;
}

static void *
sasl_mutex_alloc () {
    sem_t    *sem;

    if ((sem = lib_malloc (sizeof *sem)) != NULL)
        SEM_INIT (sem, 1);

    return sem;
}

static int
sasl_mutex_lock (void *mutex) {
    sem_t *sem = (sem_t *) mutex;

    if (!sem)
        return (-1);
    SEM_WAIT (sem);

    return 0;
}

static int
sasl_mutex_unlock (void *mutex) {
    sem_t *sem = (sem_t *) mutex;

    if (!sem)
        return (-1);
    SEM_POST (sem);

    return 0;
}

static void
sasl_mutex_free (void *mutex) {
    sem_t *sem = (sem_t *) mutex;

    if (sem != NULL)
        SEM_DESTROY (sem);
}

void cyrus_first () {
    static int once_only = 1;

    if (once_only) {
        once_only = 0;

        sasl_set_alloc (sasl_malloc, sasl_calloc, sasl_realloc, sasl_free);
        sasl_set_mutex (sasl_mutex_alloc, sasl_mutex_lock, sasl_mutex_unlock,
                        sasl_mutex_free);

        SEM_INIT (&global_sem, 1);
        global_bp = NULL;
        global_mechlist = NULL;
    }
}

PROFILE_REGISTRATION *cyrus_profiles_Init2 (struct configobj *appconfig) {
    int      result;
    char    *cp;

    cyrus_first ();

    if (!(cp = config_get (appconfig, BEEP_IDENT)))
        cp = "beepd";
    if ((result = sasl_server_init (cyrus_server_callbacks (appconfig), cp))
            != SASL_OK) {
        log_line (LOG_PROF, 7, "sasl_server_init failed (cyrus code %d) %s",
                  result, sasl_errstring (result, NULL, NULL));

        return NULL;
    }

    return cyrus_profiles_Init (appconfig);
}
