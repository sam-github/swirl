/* profile for SYSLOG reliable RAW */

/*
 * $Id: syslog-raw.c,v 1.24 2002/01/01 00:25:15 mrose Exp $
 */

#define BEEPD


/*    includes */

#include "syslog-raw.h"
#include "../threaded_os/utility/logutil.h"


/*    defines and typedefs */

#define PRO_RAW_URI             "http://iana.org/beep/SYSLOG/RAW"

#define DEFAULT_CONTENT_TYPE    "\r\n"


typedef struct pro_localdata {
    int          pl_flags;              /* mode flags                   */
#define PRO_INITIATOR   (1<<0)          /* either initiator             */
#define PRO_LISTENER    (1<<1)          /*   or listener                */
#define PRO_ONCEONLY    (1<<2)          /* report only first problem    */
#define PRO_READY       (1<<3)          /* ready for sr_log/sr_fin      */
#define PRO_ABORTED     (1<<4)          /* the session is aborting      */

    long         pl_msgNo;              /* when answering a MSG         */
    long         pl_ansNo;              /*   ..                         */

    PROFILE_INSTANCE
                *pl_pi;                 /* hook to wrappers             */

    char        *pl_serverName;         /* what you'd think             */

    sr_callback  pl_callback;           /* application callback         */
    void        *pl_clientData;         /*   ..                         */
}  PRO_LOCALDATA;


/*    statics */

static int      pro_debug;

static void     pro_upcall (PRO_LOCALDATA       *il,
                            int                  code,
                            char                *message);

static DIAGNOSTIC *
                pro_send_start (BP_CONNECTION   *bp,
                                PRO_LOCALDATA   *il);

static void     pro_send_error (PROFILE_INSTANCE *pi,
                                long            msgNo,
                                int             code,
                                char            *language,
                                char            *diagnostic);

static void     pro_syslog (PRO_LOCALDATA       *il,
                            char                *buffer,
                            int                  size);

/*    profile methods */

/*
   first four methods are invoked when channels are not-yet/no-longer
   instantiated

   success: return NULL
   failure: return char *reason
 */



/* invoked when wrapper tries to register the profile

   do process-wide initialization

   on failure, the wrapper doesn't register the profile
 */

static char *
pro_connection_init (PROFILE_REGISTRATION  *pr,
                     BP_CONNECTION         *bp) {
    if (pro_debug) {
        fprintf (stderr, "SR connection_init\n");
        fflush (stderr);
    }

    return NULL;
}


/* invoked if profile is about to be made available for the session

   do session-wide initialization

   on failure, the wrapper makes no further calls to the profile for
               the duration of the session
 */

static char *
pro_session_init (PROFILE_REGISTRATION  *pr,
                  BP_CONNECTION         *bp) {
    if (pro_debug) {
        fprintf (stderr, "SR session_init: role='%c' mode=\"%s\"\n",
                 bp -> role, bp -> mode);
        fflush (stderr);
    }

    return NULL;
}


/* invoked just before the session is released

   do session-wide finalization

   return value is ignored by wrapper
 */

static char *
pro_session_fin (PROFILE_REGISTRATION   *pr,
                 BP_CONNECTION          *bp) {
    if (pro_debug) {
        fprintf (stderr, "SR session_fin\n");
        fflush (stderr);
    }

    return NULL;
}


/* invoked just before wrapper is destroyed

   do process-wide finalization

   return value is ignored by wrapper
 */

static char *
pro_connection_fin (PROFILE_REGISTRATION   *pr,
                    BP_CONNECTION          *bp) {
    if (pro_debug) {
        fprintf (stderr, "SR connection_fin\n");
        fflush (stderr);
    }

    return NULL;
}

/* next four methods are invoked when a session/channel starts */

/* invoked when the peer sends a greeting

   listener: do nothing

   initiator: clone PRO_LOCALDATA and start a channel,
              or tell the application why not
 */

static void
pro_greeting_notification (PROFILE_REGISTRATION *pr,
                           BP_CONNECTION        *bp,
                           char                  status) {
    DIAGNOSTIC          *d;
    PRO_LOCALDATA       *pl = pr -> user_ptr,
                        *il;

    if (pro_debug) {
        fprintf (stderr, "SR greeting_notification status=%c\n",
                 status);
        fflush (stderr);
    }

    if (pl -> pl_flags & PRO_LISTENER)
        return;

    switch (status) {
        case PROFILE_PRESENT:
            break;

        case PROFILE_ABSENT:
            pro_upcall (pl, 500, "greeting: profile not present");
            return;

        case GREETING_ERROR:
            pro_upcall (pl, 500, "greeting: got error");
            return;
    }

    if (!(il = (PRO_LOCALDATA *) lib_malloc (sizeof *il))) {
        pro_upcall (pl, 500, "SR greeting: out of memory");
        return;
    }
    memcpy (il, pl, sizeof *il);

    if ((d = pro_send_start (bp, il)) != NULL) {
        pro_upcall (pl, d -> code, d -> message);
        bp_diagnostic_destroy (bp, d);
        lib_free (il);
    }
}


/* invoked when the peer asks to start a channel

   listener: ignore any piggyback'd data
             clone PRO_LOCALDATA and accept it
             or tell the application why not

   initiator: decline it
 */

static void
pro_start_indication (PROFILE_INSTANCE  *pi,
                      PROFILE           *po) {
    long                wsize;
    char                *buffer,
                        *cp;
    DIAGNOSTIC           ds,
                        *d = &ds;
    PRO_LOCALDATA       *pl = pi -> channel -> profile_registration
                                 -> user_ptr,
                        *il;
    struct configobj *appconfig = bp_get_config (pi -> channel -> conn);

    if (pro_debug) {
        fprintf (stderr, "SR start_indication: piggyback=\"%s\"\n",
                 po -> piggyback ? po -> piggyback : "<NULL>");
        fflush (stderr);
    }

    memset (d, 0, sizeof *d);

    if (pl -> pl_flags & PRO_INITIATOR) {
        memset (d, 0, sizeof *d);
        d -> code = 521;
        d -> message = "not available";
        bpc_start_response (pi -> channel, po, d);
        return;
    }

    if (!(il = (PRO_LOCALDATA *) lib_malloc (sizeof *il))) {
        pro_upcall (pl, d -> code = 421, d -> message = "out of memory");
        bpc_start_response (pi -> channel, po, d);
        return;
    }
    memcpy (il, pl, sizeof *il);
    il -> pl_pi = pi, pi -> user_ptr1 = il;

    bpc_start_response (pi -> channel, po, NULL);

    if ((cp = config_get (appconfig, SYSLOG_RAW_WINDOWSIZE))
            && (wsize = (long) atol (cp)) > 4096)
      (void) bpc_set_channel_window (pi -> channel, wsize);

    if (!(buffer = bpc_buffer_allocate (pi -> channel,
                                        sizeof DEFAULT_CONTENT_TYPE - 1))) {
        pro_upcall (il, 500, "SR start: out of memory");
        return;
    }

    strcpy (buffer, DEFAULT_CONTENT_TYPE);
    bpc_send (pi -> channel, BLU_FRAME_TYPE_MSG, BLU_FRAME_MSGNO_UNUSED,
              BLU_FRAME_IGNORE_ANSNO, BLU_FRAME_COMPLETE, buffer,
              strlen (buffer));

    (*il -> pl_callback) ((void *) il, 350, NULL, il -> pl_clientData);
}


/* invoked when a our request to start a channel has resulted in the channel starting

   remember PRO_LOCALDATA passed as clientData
 */

static void
pro_start_confirmation (void                    *clientData,
                        PROFILE_INSTANCE        *pi,
                        PROFILE                 *po) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) clientData;

    if (pro_debug) {
        fprintf (stderr, "SR start_confirmation: piggyback=\"%s\"\n",
                 po -> piggyback ? po -> piggyback : "<NULL>");
        fflush (stderr);
    }

    il -> pl_pi = pi, pi -> user_ptr1 = il;
}

/* initiator only: invoked when a response is received to our request to
                   start a channel, either from pro_greeting_notification
                   (the first time we start the channel) or
                   pro_close_confirmation (when we start the channel again
                   after sr_sync)

   failure: tell the application
*/

static void
pro_start_callback (void                    *clientData,
                    CHANNEL_INSTANCE        *ci,
                    DIAGNOSTIC              *error) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) clientData;

    if (error) {
        pro_upcall (il, error -> code, error -> message);
        lib_free (il);
    }
}


/* next three methods are invoked when a session/channel closes */


/*
   invoked when we or the peer asks to close a session/channel
   (or perhaps due to transport problem, etc.)

   always accept it
 */

static void
pro_close_indication (PROFILE_INSTANCE  *pi,
                      DIAGNOSTIC        *request,
                      char               origin,
                      char               scope) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (pro_debug) {
        fprintf (stderr,
                 "SR close_indication: request=[%d] \"%s\" origin=%c scope=%c\n",
                 request -> code, request -> message ? request -> message
                                                     : "<NULL>",
                 origin, scope);
        fflush (stderr);
    }
               
    switch (scope) {
        case PRO_ACTION_SESSION:
        case PRO_ACTION_CHANNEL:
            break;

        case PRO_ACTION_ABORT:
        default:
            il -> pl_flags |= PRO_ABORTED;
            break;
    }

    il -> pl_flags &= ~PRO_READY;
    bpc_close_response (pi -> channel, NULL);
}


/* invoked when we know whether the channel has finally closed or not
   (regardless of who requested it)

   if nothing happened, do nothing

   initiator: if flushing a channel, start another one

   otherwise, tell the application we're done
 */

static void
pro_close_confirmation (PROFILE_INSTANCE        *pi,
                        char                     status,
                        DIAGNOSTIC              *error,
                        char                     origin,
                        char                     scope) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (pro_debug) {
        if (error)
            fprintf (stderr,
                     "SR close_confirmation: status=%c error=[%d] \"%s\" origin=%c scope=%c\n",
                     status, error -> code, error -> message ? error -> message
                                                             : "<NULL>",
                     origin, scope);
        else 
            fprintf (stderr,
                     "SR close_confirmation: status=%c no error origin=%c scope=%c\n",
                     status, origin, scope);
        fflush (stderr);
    }

    if (status != PRO_ACTION_SUCCESS) {
        if (il -> pl_flags & PRO_INITIATOR)
            il -> pl_flags |= PRO_READY;
        return;
    }

    pi -> user_ptr1 = NULL;

    if (il -> pl_flags & PRO_ABORTED)
        pro_upcall (il, 421, "session aborted");
    else
        pro_upcall (il, 220, NULL);

    lib_free (il);
}


/* listener: invoked when a response is received to pro_message_available's
   request to close the channel

   success: do nothing

   failure: tell the application
*/

static void
pro_close_callback (void                *clientData,
                    CHANNEL_INSTANCE    *ci,
                    DIAGNOSTIC          *error) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) clientData;

    if (error)
        pro_upcall (il, error -> code, error -> message);
}


/* next two methods are invoked on tuning resets */


/* invoked when the local peer is about to do a tuning reset */

static void
pro_tuning_reset_indication (PROFILE_INSTANCE  *pi) {
    if (pro_debug) {
        fprintf (stderr, "SR tuning_reset_indication\n");
        fflush (stderr);
    }
}


/* invoked when we know whether the tuning reset occurred */

static void
pro_tuning_reset_confirmation (PROFILE_INSTANCE        *pi,
                               char                     status) {
    if (pro_debug) {
        fprintf (stderr, "SR tuning_reset_confirmation: status=%c\n", status);
        fflush (stderr);
    }

    pro_close_confirmation (pi, status, NULL, PRO_ACTION_LOCAL,
                            PRO_ACTION_SESSION);
}


/* next two methods are invoked on read events */


/* invoked when a message is available to be read

   message              actions taken by
   received     listener                initiator
   --------     --------                ---------
   MSG          send ERR                tell the application we're ready
                                        for sr_log

   ANS          parse and pass up       n/a

   NUL          close channel           n/a

   RPY/ERR      abort channel           n/a

 */

static void
pro_message_available (PROFILE_INSTANCE *pi) {
    int                  code = 200,
                         size;
    char                *buffer;
    DIAGNOSTIC          *d;
    FRAME               *f;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    BP_CONNECTION       *bp = pi -> channel -> conn;

    if (!(f = bpc_query_message (pi -> channel, BLU_QUERY_ANYTYPE,
                                 BLU_QUERY_ANYMSG, BLU_QUERY_ANYANS)))
      return;

    if (pro_debug) {
        fprintf (stderr,
                 "SR message_available: type=%c number=%ld answer=%ld more=%c size=%ld\n",
                 f -> msg_type, f -> message_number, f -> answer_number,
                 f -> more, f -> size);
        fprintf (stderr, f -> size > 75 ? "%-75.75s...\n" : "%s\n",
                 f -> payload);
        fflush (stderr);
    }

    switch (f -> msg_type) {
        case BLU_FRAME_TYPE_MSG:
            if (il -> pl_flags & PRO_INITIATOR) {
                il -> pl_msgNo = f -> message_number;
                il -> pl_ansNo = -1;

                il -> pl_flags |= PRO_READY;
                (*il -> pl_callback) ((void *) il, 350, NULL, il -> pl_clientData);
            } else
                pro_send_error (pi, f -> message_number, 500, NULL,
                                "not expecting MSG");
            break;

        case BLU_FRAME_TYPE_ANS:
            size = bpc_frame_aggregate (pi -> channel, f, &buffer);
            if (buffer) {
                pro_syslog (il, buffer, size);
                bpc_buffer_destroy (pi -> channel, buffer);
            } else
                bp_log (bp, LOG_PROF, 5,
                        "SR message: out of memory reading ANS");
            break;

        case BLU_FRAME_TYPE_RPY:
        case BLU_FRAME_TYPE_ERR:
            code = 500;
            il -> pl_flags |= PRO_ABORTED;
            /* and fall... */
        case BLU_FRAME_TYPE_NUL:
            if ((d = bpc_close_request (pi -> channel, BLU_CHAN0_MSGNO_DEFAULT,
                                        code, NULL,
                                        code == 500 ? "protocol error" : NULL,
                                        pro_close_callback,
                                        (void *) il)) != NULL) {
                pro_upcall (il, d -> code, d -> message);
                bp_diagnostic_destroy (bp, d);
            }
            break;

        default:
            break;
    }

    bpc_message_destroy (pi -> channel, f);
}


/* invoked when input queue is full, and the message is incomplete

   could avoid this if we used the frame-based interface

   however that would mean we'd have to be able to re-assemble multiple
   ANS messages and add a lot of state to pro_syslog...
 */

static void
pro_window_full (PROFILE_INSTANCE       *pi) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (pro_debug) {
        fprintf (stderr, "SR window_full\n");
        fflush (stderr);
    }

    pro_upcall (il, 421, "session blocked, increase window size");
}


/*    helper methods */


/* invoke the application's callback, at most once */

static void
pro_upcall (PRO_LOCALDATA       *il,
            int                  code,
            char                *message) {
    if (!(il -> pl_flags & PRO_ONCEONLY)) {
        il -> pl_flags |= PRO_ONCEONLY;

        (*il -> pl_callback) (NULL, code, message, il -> pl_clientData);
    }
}


/* initiator: start a channel */

static DIAGNOSTIC *
pro_send_start (BP_CONNECTION   *bp,
                PRO_LOCALDATA   *il) {
    PROFILE              ps,
                        *p = &ps;
    struct configobj *appconfig = bp_get_config (bp);

    memset (p, 0, sizeof *p);
    if (!(p -> uri = config_get (appconfig, SYSLOG_RAW_URI)))
        p -> uri = PRO_RAW_URI;
    p -> piggyback_length = strlen (p -> piggyback = "");

    return bp_start_request (bp, BLU_CHAN0_CHANO_DEFAULT,
                             BLU_CHAN0_MSGNO_DEFAULT, p, il -> pl_serverName,
                             pro_start_callback, (void *) il);
}


/* listener: send an ERR if we receive a MSG */

static void
pro_send_error (PROFILE_INSTANCE        *pi,
                long                     msgNo,
                int                      code,
                char                    *language,
                char                    *diagnostic) {
    char *buffer = bpc_error_allocate (pi -> channel, code, language,
                                       diagnostic);

    if (buffer)
        bpc_send (pi -> channel, BLU_FRAME_TYPE_ERR, msgNo,
                  BLU_FRAME_IGNORE_ANSNO, BLU_FRAME_COMPLETE, buffer,
                  strlen (buffer));
    else
        bp_log (pi -> channel -> conn, LOG_PROF, 5,
                "SR message: out of memory sending ERR");
}


/* listener: parse syslog entries and pass to application */

static void
pro_syslog (PRO_LOCALDATA       *il,
            char                *buffer,
            int                  size) {
    int          twiceP;
    char        *cp,
                *dp;

    /* skip past MIME headers to find beginning of data, two possibilities:

       Content-Type: ...\r\n[...\r\n]\r\n

       \r\n

    */

    cp = buffer;
    twiceP = 1;
    while (size-- > 0) {
        char    c = *cp++;

        switch (c) {
            case '\r':
                if ((size > 0) && (*cp == '\n'))
                    cp++, size--;
                twiceP++;
                break;

            case '\n':
                twiceP++;
                break;

            default:
              twiceP = 0;
                break;
        }
        if (twiceP > 1)
            break;
    }
            
    dp = cp;
    while (size > 0) {
        char    c = *cp;

        switch (c) {
            case '\r':
            case '\n':
                *cp = '\0';
                if (cp > dp)
                    (*il -> pl_callback) ((void *) il, cp - dp, dp,
                                          il -> pl_clientData);
                cp++, size--;
                if ((size > 0) && (*cp == '\n'))
                    cp++, size--;
                dp = cp;
                break;

            default:
                cp++, size--;
                break;
        }
    }
    if (cp > dp)
        (*il -> pl_callback) ((void *) il, cp - dp, dp, il -> pl_clientData);
}


/*
 * Well-known shared library entry point.
 */

PROFILE_REGISTRATION *
sr_Init (struct configobj  *appconfig) {
    char                 *cp;
    PRO_LOCALDATA        *pl;
    PROFILE_REGISTRATION *pr;

    pro_debug = 0;
    if ((cp = config_get (appconfig, SYSLOG_RAW_DEBUG)) != NULL)
        pro_debug = atoi (cp);

    if (!(pr = (PROFILE_REGISTRATION *) lib_malloc (sizeof *pr +
                                                    sizeof *pl))) {
        log_line (LOG_PROF, 6, "unable to allocate PR");
        return NULL;
    }
    memset (pr, 0, sizeof *pr);

    if (!(pr -> uri = config_get (appconfig, SYSLOG_RAW_URI)))
        pr -> uri = PRO_RAW_URI;

    if (!(pr -> initiator_modes = config_get (appconfig, SYSLOG_RAW_IMODES)))
        pr -> initiator_modes = "plaintext,encrypted";
    if (!(pr -> listener_modes = config_get (appconfig, SYSLOG_RAW_LMODES)))
        pr -> listener_modes = "plaintext,encrypted";

    pr -> full_messages = 1;
    pr -> thread_per_channel = 0;

    pr -> proreg_connection_init = pro_connection_init;
    pr -> proreg_connection_fin  = pro_connection_fin;
    pr -> proreg_session_init = pro_session_init;
    pr -> proreg_session_fin  = pro_session_fin;
    pr -> proreg_greeting_notification = pro_greeting_notification;
    pr -> proreg_start_indication   = pro_start_indication;
    pr -> proreg_start_confirmation = pro_start_confirmation;
    pr -> proreg_close_indication   = pro_close_indication;
    pr -> proreg_close_confirmation = pro_close_confirmation;
    pr -> proreg_tuning_reset_indication   = pro_tuning_reset_indication;
    pr -> proreg_tuning_reset_confirmation = pro_tuning_reset_confirmation;
    pr -> proreg_message_available = pro_message_available;
    pr -> proreg_window_full       = pro_window_full;

    pr -> user_ptr = pl = (PRO_LOCALDATA *) (((char *) pr) + sizeof *pr);
    memset (pl, 0, sizeof *pl);

    return pr;
}
 

/* invoked by a listening application 

               pr - non-NULL result from sr_Init
         callback - pointer to callback procedure
       clientData - opaque pointer
 */

void
sr_listener (PROFILE_REGISTRATION       *pr,
             sr_callback                 callback,
             void                       *clientData) {
    PRO_LOCALDATA       *pl = pr -> user_ptr;

    pl -> pl_flags = PRO_LISTENER;
    pl -> pl_callback = callback;
    pl -> pl_clientData = clientData;
}


/* invoked by an initiating application 

               pr - non-NULL result from sr_Init
         callback - pointer to callback procedure
       clientData - opaque pointer
 */

void
sr_initiator (PROFILE_REGISTRATION      *pr,
              char                      *serverName,
              sr_callback                callback,
              void                      *clientData) {
    PRO_LOCALDATA       *pl = pr -> user_ptr;

    pl -> pl_flags = PRO_INITIATOR;
    pl -> pl_serverName = serverName;
    pl -> pl_callback = callback;
    pl -> pl_clientData = clientData;
}


/* initiator: send syslog entry */

int
sr_log (void                    *v,
        char                    *entry) {
    int                  i;
    char                *buffer,
                        *ep;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) v;
    PROFILE_INSTANCE    *pi;

    if (!(pi = il -> pl_pi)) {
        lib_free (il);
        return SR_DONE;
    }
    if (!(il -> pl_flags & PRO_READY))
        return SR_BUSY;

    if ((i = strlen (ep = entry)) > 0) {
        ep += i - 1;
        if (*ep == '\n')
           ep--, i--;
        if ((i > 0) && (*ep == '\r'))
           i--;
    }
    if (i == 0)
        return SR_OK;
    
    if (!(buffer = bpc_buffer_allocate (pi -> channel,
                                         i
                                       + (sizeof DEFAULT_CONTENT_TYPE - 1))))
        return SR_ERROR;
    sprintf (buffer, "%s%*.*s", DEFAULT_CONTENT_TYPE, i, i, entry);

    if (++il -> pl_ansNo > 2147483647)
        il -> pl_ansNo = 0;
    bpc_send (pi -> channel, BLU_FRAME_TYPE_ANS, il -> pl_msgNo,
              il -> pl_ansNo, BLU_FRAME_COMPLETE, buffer, strlen (buffer));

    return SR_OK;
}


/* initiator: drain entries, optionally closing channel */

int
sr_fin (void                  *v) {
    char                *buffer;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) v;
    PROFILE_INSTANCE    *pi;

    if (!(pi = il -> pl_pi)) {
        lib_free (il);
        return SR_DONE;
    }
    if (!(il -> pl_flags & PRO_READY))
        return SR_BUSY;

    if (!(buffer = bpc_buffer_allocate (pi -> channel, 0)))
        return SR_ERROR;

    il -> pl_flags &= ~PRO_READY;
    bpc_send (pi -> channel, BLU_FRAME_TYPE_NUL, il -> pl_msgNo,
              BLU_FRAME_IGNORE_ANSNO, BLU_FRAME_COMPLETE, buffer, 0);

    return SR_OK;
}

#ifdef  BEEPD
static void
callback_listener (void *v,
                   int   code,
                   char *diagnostic,
                   void *clientData) {
    fprintf (stderr,
             "SR callback_listener v=0x%x code=%d\n   diagnostic=\"%s\"\n",
             (unsigned int) v, code, diagnostic);
    fflush (stderr);
}

PROFILE_REGISTRATION *
sr_Init_listener (struct configobj  *appconfig) {
    PROFILE_REGISTRATION *pr = sr_Init (appconfig);

    if (pr)
        sr_listener (pr, callback_listener, NULL);

    return pr;
}
#endif
