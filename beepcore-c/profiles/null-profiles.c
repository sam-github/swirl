/* profiles for NULL ECHO/SINK */

/*
 * $Id: null-profiles.c,v 1.19 2002/01/20 23:03:40 mrose Exp $
 */


/*    includes */

#ifndef WIN32
#include <string.h>
#else
#include <windows.h>
#endif
#include "null-profiles.h"
#include "tuning.h"
#include "../threaded_os/utility/logutil.h"
#include "../threaded_os/utility/semutex.h"


/*    defines and typedefs */

#define PRO_ECHO_URI            "http://xml.resource.org/profiles/NULL/ECHO"
#define PRO_SINK_URI            "http://xml.resource.org/profiles/NULL/SINK"

#define EMPTY_RPY \
        "Content-Type: application/beep+xml\r\n\r\n<null />"


typedef struct pro_localdata {
    int          pl_flags;              /* mode flags                   */
#define PRO_ECHOMODE    (1<<0)          /* doing echo, not sink         */
#define PRO_START       (1<<1)          /* did null_start               */
#define PRO_READY       (1<<2)          /* ready for null_send          */
#define PRO_RSPWAIT     (1<<3)          /* waiting for peer's response  */
#define PRO_CLSWAIT     (1<<4)          /* waiting for close            */
#define PRO_DENIED      (1<<5)          /* close request was denied     */
#define PRO_DEBUG       (1<<6)          /* debugging                    */

    PROFILE_INSTANCE
                *pl_pi;                 /* return value for null_start  */

    char        *pl_rbuf;               /* out parameters for null_trip */
    int          pl_rlen;               /*   ..                         */

    sem_t        pl_sem;                /* semaphore used for receive */
}  PRO_LOCALDATA;


/*    statics */

static char    *pro_name (char *uri);


/*    profile methods */

static char *
pro_name (char *uri) {
    char        *cp;

    if ((cp = strrchr (uri, '/')) != NULL)
        return ++cp;
    return uri;
}


/* first four methods are invoked when channels are not-yet/no-longer
   instantiated

   success: return NULL
   failure: return char *reason
 */


/* invoked when profile is registered with the wrapper

   do process-wide initialization

   on failure, the wrapper makes no further calls to the profile for
               the duration of the session
 */

static char *
pro_connection_init (PROFILE_REGISTRATION  *pr,
                     BP_CONNECTION         *bp) {
    if (((PRO_LOCALDATA *) pr -> user_ptr) -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s connection_init\n", pro_name (pr -> uri));
        fflush (stderr);
    }

    return NULL;
}


/* invoked if profile is about to be made available for the session

   do session-wide initialization

   on failure, the wrapper makes no further calls to the profile
 */

static char *
pro_session_init (PROFILE_REGISTRATION  *pr,
                  BP_CONNECTION         *bp) {
    if (((PRO_LOCALDATA *) pr -> user_ptr) -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s session_init: role='%c' mode=\"%s\"\n",
                 pro_name (pr -> uri), bp -> role, bp -> mode);
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
    if (((PRO_LOCALDATA *) pr -> user_ptr) -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s session_fin\n", pro_name (pr -> uri));
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
    if (((PRO_LOCALDATA *) pr -> user_ptr) -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s connection_fin\n", pro_name (pr -> uri));
        fflush (stderr);
    }

    return NULL;
}


/* next three methods are invoked when a session/channel starts */


/* invoked when the peer asks to start a channel

   if not echoing, ignore any piggyback'd data  
   clone PRO_LOCALDATA and accept it
 */

static void
pro_start_indication (PROFILE_INSTANCE  *pi,
                      PROFILE           *po) {
    DIAGNOSTIC           ds,
                        *d = &ds;
    PRO_LOCALDATA       *pl = pi -> channel -> profile_registration
                                 -> user_ptr,
                        *il;
    PROFILE              ps,
                        *p = &ps;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    if (pl -> pl_flags & PRO_DEBUG) {
        char    *cp,
               **pp;
        static char *pairs[] = {
            PRIVACY_REMOTE_SUBJECTNAME, "PRIVACY_REMOTE_SUBJECTNAME",
            PRIVACY_CIPHER_NAME,        "PRIVACY_CIPHER_NAME",
            PRIVACY_CIPHER_PROTOCOL,    "PRIVACY_CIPHER_PROTOCOL",
            PRIVACY_CIPHER_SBITS,       "PRIVACY_CIPHER_SBITS",
            SASL_LOCAL_MECHANISM,       "SASL_LOCAL_MECHANISM",
            SASL_LOCAL_CODE,            "SASL_LOCAL_CODE",
            SASL_LOCAL_REASON,          "SASL_LOCAL_REASON",
            SASL_REMOTE_MECHANISM,      "SASL_REMOTE_MECHANISM",
            SASL_REMOTE_USERNAME,       "SASL_REMOTE_USERNAME",
            SASL_REMOTE_TARGET,         "SASL_REMOTE_TARGET",
            SASL_REMOTE_CODE,           "SASL_REMOTE_CODE",
            SASL_REMOTE_REASON,         "SASL_REMOTE_REASON",
          NULL
        };      

        fprintf (stderr, "%s start_indication: piggyback=\"%s\"\n",
                 pro_name (po -> uri),
                 po -> piggyback ? po -> piggyback : "<NULL>");
        for (pp = pairs; *pp; pp++)
            if ((cp = config_get (appconfig, *pp++)) != NULL)
                fprintf (stderr, "    %s: \"%s\"\n", *pp, cp);

        fflush (stderr);
    }
               
    if (!(il = (PRO_LOCALDATA *) lib_malloc (sizeof *il))) {
        memset (d, 0, sizeof *d);
        d -> code = 421;
        d -> message = "out of memory";
        bpc_start_response (pi -> channel, po, d);
        return;
    }
    memcpy (il, pl, sizeof *il);
    il -> pl_pi = pi, pi -> user_ptr1 = il;

    if (il -> pl_flags & PRO_ECHOMODE)
        p = po;
    else {
        memset (p, 0, sizeof *p);
        p -> uri = po -> uri;
    }

    bpc_start_response (pi -> channel, p, NULL);
}


/* invoked when a our request to start a channel has resulted in the channel starting

   remember PRO_LOCALDATA passed as clientData
 */

static void
pro_start_confirmation (void             *clientData,
                        PROFILE_INSTANCE        *pi,
                        PROFILE                 *po) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) clientData;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    if (il -> pl_flags & PRO_DEBUG) {
        char    *cp,
               **pp;
        static char *pairs[] = {
            PRIVACY_REMOTE_SUBJECTNAME, "PRIVACY_REMOTE_SUBJECTNAME",
            PRIVACY_CIPHER_NAME,        "PRIVACY_CIPHER_NAME",
            PRIVACY_CIPHER_PROTOCOL,    "PRIVACY_CIPHER_PROTOCOL",
            PRIVACY_CIPHER_SBITS,       "PRIVACY_CIPHER_SBITS",
            SASL_LOCAL_MECHANISM,       "SASL_LOCAL_MECHANISM",
            SASL_LOCAL_CODE,            "SASL_LOCAL_CODE",
            SASL_LOCAL_REASON,          "SASL_LOCAL_REASON",
            SASL_REMOTE_MECHANISM,      "SASL_REMOTE_MECHANISM",
            SASL_REMOTE_USERNAME,       "SASL_REMOTE_USERNAME",
            SASL_REMOTE_TARGET,         "SASL_REMOTE_TARGET",
            SASL_REMOTE_CODE,           "SASL_REMOTE_CODE",
            SASL_REMOTE_REASON,         "SASL_REMOTE_REASON",
          NULL
        };      

        fprintf (stderr, "%s start_confirmation: piggyback=\"%s\"\n",
                 pro_name (po -> uri),
                 po -> piggyback ? po -> piggyback : "<NULL>");
        for (pp = pairs; *pp; pp++)
            if ((cp = config_get (appconfig, *pp++)) != NULL)
                fprintf (stderr, "    %s: \"%s\"\n", *pp, cp);

        fflush (stderr);
    }

    il -> pl_pi = pi, pi -> user_ptr1 = il;
}


/* invoked when a response is received to null_start's request to start a channel

   note failure
*/

static void
pro_start_callback (void                *clientData,
                    CHANNEL_INSTANCE    *ci,
                    DIAGNOSTIC          *error) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) clientData;

    if (il -> pl_flags & PRO_DEBUG) {
      if (error)
          fprintf (stderr, "%s pro_start_callback: error=[%d] \"%s\" \n",
                   pro_name (ci -> profile_registration -> uri),
                   error -> code,
                   error -> message ? error -> message : "<NULL>");
      else
          fprintf (stderr, "%s pro_start_callback: no error\n",
                   pro_name (ci -> profile_registration -> uri));
        fflush (stderr);
    }

    if (error)
        printf ("unable to start channel: [%d] %s\n",
                   error -> code,
                   error -> message ? error -> message : "<NULL>");
}


/* next three methods are invoked when a session/channel closes */


/*
   invoked when we or the peer asks to close a session/channel
   (or perhaps due to transport problem, etc.)

   if the remote peer is asking, reject it

   otherwise, accept it
 */

static void
pro_close_indication (PROFILE_INSTANCE  *pi,
                      DIAGNOSTIC        *request,
                      char               origin,
                      char               scope) {
    BP_CONNECTION       *bp = pi -> channel -> conn;
    DIAGNOSTIC          *d;
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
               
    d = ((il -> pl_flags & PRO_START) && (origin == PRO_ACTION_REMOTE))
            ? bp_diagnostic_new (bp, 500, NULL, "no thanks")
            : NULL;

    bpc_close_response (pi -> channel, d);

    if (d)
        bp_diagnostic_destroy (bp, d);
}


/* invoked when we know whether the channel has finally closed or not
   (regardless of who requested it)

   if nothing happened, do nothing

   if we requested the close, leave PRO_LOCALDATA for null_close

   if we're doing a round-trip, signal null_trip and release the semaphore

   otherwise, lib_free PRO_LOCALDATA
 */

static void
pro_close_confirmation (PROFILE_INSTANCE        *pi,
                        char                     status,
                        DIAGNOSTIC              *error,
                        char                     origin,
                        char                     scope) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

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

    pi -> user_ptr1 = NULL;

    if (il -> pl_flags & PRO_CLSWAIT)
        il -> pl_flags &= ~PRO_CLSWAIT;
    else if (il -> pl_flags & PRO_START) {
        il -> pl_pi = NULL;
        if (il -> pl_flags & PRO_RSPWAIT) {
            il -> pl_flags &= ~PRO_RSPWAIT;
            SEM_POST (&il -> pl_sem);
        }
    }
    else
        lib_free (il);
}


/* invoked when a response is received to null_close's request to close
   the channel

   note failure
*/

static void
pro_close_callback (void                *clientData,
                    CHANNEL_INSTANCE    *ci,
                    DIAGNOSTIC          *error) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) clientData;

    if (il -> pl_flags & PRO_DEBUG) {
      if (error)
          fprintf (stderr, "%s pro_close_callback: error=[%d] \"%s\" \n",
                   pro_name (ci -> profile_registration -> uri),
                   error -> code,
                   error -> message ? error -> message : "<NULL>");
      else
          fprintf (stderr, "%s pro_close_callback: no error\n",
                   pro_name (ci -> profile_registration -> uri));
        fflush (stderr);
    }

    if (error) {
        il -> pl_flags |= PRO_DENIED, il -> pl_flags &= ~PRO_CLSWAIT;

        printf ("unable to close channel: [%d] %s\n",
                error -> code,
                error -> message ? error -> message : "<NULL>");
    }
}


/* next two methods are invoked on tuning resets */


/* invoked when the local peer is about to do a tuning reset */

static void
pro_tuning_reset_indication (PROFILE_INSTANCE  *pi) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr, "%s tuning_reset_indication\n",
                 pro_name (pi -> channel -> profile_registration -> uri));
        fflush (stderr);
    }
}


/* invoked when we know whether the tuning reset occurred */

static void
pro_tuning_reset_confirmation (PROFILE_INSTANCE        *pi,
                               char                     status) {
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


/* invoked when a frame is available to be read

      frame         actions taken by
   received     ECHO                SINK
   --------     ----                ----
        MSG     send echo RPY       partial: ignore
                                    complete: send empty RPY

        RPY     update out parameters for null_trip
                if complete, leave RSPWAIT

    ERR/NUL     if complete, leave RSPWAIT

        ANS     ignore              ignore  

 */

static void
pro_frame_available (PROFILE_INSTANCE   *pi) {
    int                  size;
    char                *buffer,
                        *payload;
    FRAME               *f;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (!(f = bpc_query_frame (pi -> channel, BLU_QUERY_ANYTYPE,
                               BLU_QUERY_ANYMSG, BLU_QUERY_ANYANS)))
      return;

    if (il -> pl_flags & PRO_DEBUG) {
        fprintf (stderr,
                 "%s frame_available: type=%c number=%ld answer=%ld more=%c size=%ld\n",
                 pro_name (pi -> channel -> profile_registration -> uri),
                 f -> msg_type, f -> message_number, f -> answer_number,
                 f -> more, f -> size);
        fprintf (stderr, f -> size > 75 ? "%-75.75s...\n" : "%s\n",
                 f -> payload);
        fflush (stderr);
    }

    switch (f -> msg_type) {
        case BLU_FRAME_TYPE_MSG:
            if (il -> pl_flags & PRO_ECHOMODE)
                payload = f -> payload, size = f -> size;
            else if (f -> more == BLU_FRAME_COMPLETE)
                payload = EMPTY_RPY, size = sizeof EMPTY_RPY - 1;
            else
                break;
            if ((buffer = bpc_buffer_allocate (pi -> channel, size)) != NULL) {
                memcpy (buffer, payload, size);
                bpc_send (pi -> channel, BLU_FRAME_TYPE_RPY,
                          f -> message_number, BLU_FRAME_IGNORE_ANSNO,
                          f -> more, buffer, size);
            } else
                bp_log (pi -> channel -> conn, LOG_PROF, 5,
                        "%s frame_available: out of memory",
                        pro_name (pi -> channel -> profile_registration -> uri));
            break;

        case BLU_FRAME_TYPE_RPY:
            if (f -> size <= il -> pl_rlen) {
                memcpy (il -> pl_rbuf, f -> payload, f -> size);
                il -> pl_rbuf += f -> size, il -> pl_rlen -= f -> size;
            } else
                il -> pl_rlen = -1;
            /* and fall... */
        case BLU_FRAME_TYPE_ERR:
        case BLU_FRAME_TYPE_NUL:
            if (f -> more == BLU_FRAME_COMPLETE) {
                il -> pl_flags &= ~PRO_RSPWAIT;
                SEM_POST (&il -> pl_sem);
            }
            break;

        case BLU_FRAME_TYPE_ANS:
        default:
            break;
    }

    bpc_frame_destroy (pi -> channel, f);
}


/*    module entry points */

static PROFILE_REGISTRATION *
pro_init (struct configobj  *appconfig,
          int                echoP) {
    int                   debugP;
    char                 *cp;
    PRO_LOCALDATA        *pl;
    PROFILE_REGISTRATION *pr;

    if (!(pr = (PROFILE_REGISTRATION *) lib_malloc (sizeof *pr +
                                                    sizeof *pl))) {
        log_line (LOG_PROF, 6, "unable to allocate PR");
        return NULL;
    }
    memset (pr, 0, sizeof *pr);

    cp = echoP ? NULL_ECHO_DEBUG : NULL_SINK_DEBUG;
    debugP = 0;
    if ((cp = config_get (appconfig, cp)) != NULL)
        debugP = atoi (cp);

    cp = echoP ? NULL_ECHO_URI : NULL_SINK_URI;
    if (!(pr -> uri = config_get (appconfig, cp)))
        pr -> uri = echoP ? PRO_ECHO_URI : PRO_SINK_URI;

    cp = echoP ? NULL_ECHO_IMODES : NULL_SINK_IMODES;
    if (!(pr -> initiator_modes = config_get (appconfig, cp)))
        pr -> initiator_modes = "plaintext,encrypted";
    cp = echoP ? NULL_ECHO_LMODES : NULL_SINK_LMODES;
    if (!(pr -> listener_modes = config_get (appconfig, cp)))
        pr -> listener_modes = "plaintext,encrypted";

    pr -> full_messages = 0;
    pr -> thread_per_channel = 0;

    pr -> proreg_connection_init = pro_connection_init;
    pr -> proreg_connection_fin  = pro_connection_fin;
    pr -> proreg_session_init = pro_session_init;
    pr -> proreg_session_fin  = pro_session_fin;
    pr -> proreg_start_indication   = pro_start_indication;
    pr -> proreg_start_confirmation = pro_start_confirmation;
    pr -> proreg_close_indication   = pro_close_indication;
    pr -> proreg_close_confirmation = pro_close_confirmation;
    pr -> proreg_tuning_reset_indication   = pro_tuning_reset_indication;
    pr -> proreg_tuning_reset_confirmation = pro_tuning_reset_confirmation;
    pr -> proreg_frame_available = pro_frame_available;

    pr -> user_ptr = pl = (PRO_LOCALDATA *) (((char *) pr) + sizeof *pr);
    memset (pl, 0, sizeof *pl);
    if (echoP)
        pl -> pl_flags |= PRO_ECHOMODE;
    if (debugP)
        pl -> pl_flags |= PRO_DEBUG;

    return pr;
}

PROFILE_REGISTRATION *
null_echo_Init (struct configobj  *appconfig) {
    return pro_init (appconfig, 1);
}

PROFILE_REGISTRATION *
null_sink_Init (struct configobj  *appconfig) {
    return pro_init (appconfig, 0);
}


/* initiator: start channel */

void *
null_start (BP_CONNECTION               *bp,
            PROFILE_REGISTRATION        *pr,
            char                        *serverName) {
    DIAGNOSTIC          *d;
    PRO_LOCALDATA       *pl = pr -> user_ptr,
                        *il;
    PROFILE              ps,
                        *p = &ps;

    memset (p, 0, sizeof *p);
    p -> uri = pr -> uri;
    p -> piggyback_length = strlen (p -> piggyback = "");

    if (!(il = (PRO_LOCALDATA *) lib_malloc (sizeof *il)))
        return NULL;
    memcpy (il, pl, sizeof *il);
    il -> pl_flags |= PRO_START;

    /* this blocks! */
    d = bp_start_request (bp, BLU_CHAN0_CHANO_DEFAULT, BLU_CHAN0_MSGNO_DEFAULT,
                          p, serverName, pro_start_callback, (void *) il);

    if (d) {
        printf ("unable to start channel: [%d] %s\n",
                d -> code, d -> message);
        bp_diagnostic_destroy (bp, d);
    }

    if (il -> pl_pi) {
        SEM_INIT (&il -> pl_sem, 0);
        il -> pl_flags |= PRO_READY;
    } else
        lib_free (il), il = NULL;

    return ((void *) il);
}


/* initiator: send a message, get back reply */

extern int
null_trip (void                         *v,
           char                         *ibuf,
           int                           ilen,
           char                         *obuf,
           int                           omaxlen) {
    char                *buffer;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) v;
    PROFILE_INSTANCE    *pi;

    if (!(pi = il -> pl_pi)) {
        SEM_DESTROY (&il -> pl_sem);
        lib_free (il);
        return NULL_DONE;
    }
    if (!(il -> pl_flags & PRO_READY))
        return NULL_BUSY;

    if (!(buffer = bpc_buffer_allocate (pi -> channel, ilen)))
        return NULL_ERROR;
    memcpy (buffer, ibuf, ilen);

    il -> pl_rbuf = obuf, il -> pl_rlen = omaxlen;
    il -> pl_flags |= PRO_RSPWAIT, il -> pl_flags &= ~PRO_READY;

    bpc_send (pi -> channel, BLU_FRAME_TYPE_MSG, BLU_FRAME_MSGNO_UNUSED,
              BLU_FRAME_IGNORE_ANSNO, BLU_FRAME_COMPLETE, buffer, ilen);

    SEM_WAIT (&il -> pl_sem);

    if (!il -> pl_pi) {
        SEM_DESTROY (&il -> pl_sem);
        lib_free (il);
        return NULL_DONE;
    }

    il -> pl_flags |= PRO_READY;

    if (il -> pl_rlen < 0)
        return NULL_ERROR;

    return (omaxlen - il -> pl_rlen);
}


/* initiator: close channel */

extern int
null_close (void        *v) {
    BP_CONNECTION       *bp;
    DIAGNOSTIC          *d;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) v;
    PROFILE_INSTANCE    *pi;

    if (!(pi = il -> pl_pi)) {
        SEM_DESTROY (&il -> pl_sem);
        lib_free (il);
        return NULL_DONE;
    }
    if (!(il -> pl_flags & PRO_READY))
        return NULL_BUSY;

    il -> pl_flags |= PRO_CLSWAIT, il -> pl_flags &= ~PRO_READY;

    bp = pi -> channel -> conn;
    if ((d = bpc_close_request (pi -> channel, BLU_CHAN0_MSGNO_DEFAULT, 200,
                                NULL, NULL, pro_close_callback,
                                (void *) il)) != NULL) {
        printf ("unable to close channel: [%d] %s\n",
                d -> code, d -> message);
        bp_diagnostic_destroy (bp, d);

        il -> pl_flags |= PRO_READY, il -> pl_flags &= ~PRO_CLSWAIT;
        return NULL_ERROR;
    }

    /* could use a semaphore instead of a spinlock, but it's an example... */
    while (il -> pl_flags & PRO_CLSWAIT)
        YIELD ();

    if (il -> pl_flags & PRO_DENIED) {
        il -> pl_flags |= PRO_READY, il -> pl_flags &= ~PRO_DENIED;
        return NULL_DENIED;
    }

    SEM_DESTROY (&il -> pl_sem);
    lib_free (il);
    return NULL_OK;
}
