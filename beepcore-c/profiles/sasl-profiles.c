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
 * $Id: sasl-profiles.c,v 1.9 2002/01/16 22:13:24 mrose Exp $
 */


/*    includes */

#ifndef WIN32
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
#include "sasl-profiles.h"
#include "../threaded_os/utility/logutil.h"
#include "../core/generic/CBEEPint.h"
#include "SASLutils/sasl_general.h"


/*    defines and typedefs */

#define PRO_ANONYMOUS_URI       "http://iana.org/beep/SASL/ANONYMOUS"
#define PRO_OTP_URI             "http://iana.org/beep/SASL/OTP"

#define DEFAULT_CONTENT_TYPE    "Content-Type: application/beep+xml\r\n\r\n"


typedef struct pro_localdata {
    int                  pl_flags;
#define PRO_INITIATOR   (1<<0)
#define PRO_STARTING    (1<<1)
#define PRO_CLOSED      (1<<2)

    CHANNEL_INSTANCE    *pl_channel;
    DIAGNOSTIC          *pl_status;

    int                   pl_task;
#define PRO_SERVER_ANON         0x00
#define PRO_SERVER_OTP          0x01
#define PRO_CLIENT_ANON         0x80
#define PRO_CLIENT_OTP          0x81

    int                  pl_sent;
    struct sasl_data_block
                         pl_sdb;
}  PRO_LOCALDATA;


/*    statics */

static int      pro_debug;
static char    *pro_name (char *uri);

static void     sasl_error (PROFILE_INSTANCE *pi,
                            char             *data,
                            int              size);

static char    *sasl_response (PROFILE_INSTANCE *pi,
                               int               result);

static void     sasl_set_local_success (PROFILE_INSTANCE *pi);
static void     sasl_set_local_failure (PROFILE_INSTANCE *pi,
                                        DIAGNOSTIC       *d);
static void     sasl_set_remote_success (PROFILE_INSTANCE *pi);
static void     sasl_set_remote_failure (PROFILE_INSTANCE *pi,
                                         int               code,
                                         char             *reason);


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
    if (pro_debug) {
        fprintf (stderr, "%s connection_init\n", pro_name (pr -> uri));
        fflush (stderr);
    }

    return NULL;
}

static char *
pro_session_init (PROFILE_REGISTRATION *pr,
                  BP_CONNECTION        *bp) {
    if (pro_debug) {
        fprintf (stderr, "%s session_init: role='%c' mode=\"%s\"\n",
                 pro_name (pr -> uri), bp -> role, bp -> mode);
        fflush (stderr);
    }

    return NULL;
}

static char *
pro_session_fin (PROFILE_REGISTRATION *pr,
                 BP_CONNECTION        *bp) {
    if (pro_debug) {
        fprintf (stderr, "%s session_fin\n", pro_name (pr -> uri));
        fflush (stderr);
    }

    return NULL;
}

static char *
pro_connection_fin (PROFILE_REGISTRATION *pr,
                    BP_CONNECTION        *bp) {
    if (pro_debug) {
        fprintf (stderr, "%s connection_fin\n", pro_name (pr -> uri));
        fflush (stderr);
    }

    return NULL;
}

static void
pro_start_indication (PROFILE_INSTANCE *pi,
                      PROFILE          *po) {
    char                *cp,
                        *otp;
    DIAGNOSTIC           ds,
                        *d = &ds;
    PRO_LOCALDATA       *il;
    PROFILE              ps,
                        *p = &ps;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);
    struct cfgsearchobj *cs;

    if (pro_debug) {
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

    if (!(otp = config_get (appconfig, SASL_OTP_URI)))
        otp = PRO_OTP_URI;
    il -> pl_task = strcmp (po -> uri, otp) ? PRO_SERVER_ANON : PRO_SERVER_OTP;
    if (!(il -> pl_sdb.path = config_get (appconfig, SASL_OTP_DIRECTORY)))
        il -> pl_sdb.path = "/tmp";

    cs = config_search_init (appconfig, SASL_REMOTE_PREFIX, "");
    for (cp = config_search_string_firstkey (cs);
             cp;
             cp = config_search_string_nextkey (cs))
        config_delete (appconfig, cp);
    config_search_fin (&cs);
    switch (il -> pl_task) {
        case PRO_SERVER_ANON:
            config_set (appconfig, SASL_REMOTE_MECHANISM, "anonymous");
            break;

        case PRO_SERVER_OTP:
            config_set (appconfig, SASL_REMOTE_MECHANISM, "otp");
            break;
    }

    if (po -> piggyback_length > 0) {
        int     result;

        switch (il -> pl_task) {
            case PRO_SERVER_ANON:
                if ((result = sasl_anonymous_server_guts (po -> piggyback,
                                                          &il -> pl_sdb))
                        == SASL_COMPLETE)
                    config_set (appconfig, SASL_REMOTE_USERNAME,
                                il -> pl_sdb.parse_blob_data);
                break;

            case PRO_SERVER_OTP:
                result = sasl_otp_server_guts_1 (po -> piggyback,
                                                 &il -> pl_sdb);
                break;
        }
        p -> piggyback_length = strlen (p -> piggyback =
                                            sasl_response (pi, result));
        il -> pl_sent = 1;
    }

    bpc_start_response (pi -> channel, p, NULL);
}

static void
pro_start_confirmation (void             *clientData,
                        PROFILE_INSTANCE *pi,
                        PROFILE          *po) {
    int                  problem,
                         size;
    char                *buffer,
                        *payload;
    PRO_LOCALDATA       *il = clientData;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    if (pro_debug) {
        fprintf (stderr, "%s start_confirmation: piggyback=\"%s\"\n",
                 pro_name (po -> uri),
                 po -> piggyback ? po -> piggyback : "<NULL>");
        fflush (stderr);
    }

    pi -> user_ptr1 = il;

    il -> pl_flags &= ~(PRO_STARTING | PRO_CLOSED);
    il -> pl_channel = pi -> channel;

    if (po -> piggyback_length == 0) {
        il -> pl_status = bp_diagnostic_new (pi -> channel -> conn, 504, NULL,
                                             "expecting blob element");
        return;
    }

    if (strstr (po -> piggyback, "<error")) {
        sasl_error (pi, po -> piggyback, po -> piggyback_length);
        return;
    }
    
    switch (il -> pl_task) {
        case PRO_CLIENT_ANON:
            parse_blob (po -> piggyback, &il -> pl_sdb);
            if (!strcmp (il -> pl_sdb.parse_blob_status, "complete"))
                sasl_set_local_success (pi);
            else
                sasl_set_local_failure (pi, NULL);
            break;

        case PRO_CLIENT_OTP:
            if (!(payload = sasl_otp_client_guts_2
                                (po -> piggyback,
                                 config_get (appconfig, SASL_LOCAL_PASSPHRASE),
                                 &problem, &il -> pl_sdb))) {
                if (problem == SASL_ERROR_ALGORITHM) {
                    config_set (appconfig, SASL_LOCAL_CODE, "504");
                    config_set (appconfig, SASL_LOCAL_REASON,
                                "unsupported algorithm");
                } else {
                    config_set (appconfig, SASL_LOCAL_CODE, "501");
                    config_set (appconfig, SASL_LOCAL_REASON,
                                "unable to parse blob");
                }

                payload = "<blob status='abort' />";
            }
            size = strlen (payload) + sizeof DEFAULT_CONTENT_TYPE - 1;
            if ((buffer = bpc_buffer_allocate (pi -> channel, size)) != NULL) {
                sprintf (buffer, "%s%s", DEFAULT_CONTENT_TYPE, payload);

                bpc_send (pi -> channel, BLU_FRAME_TYPE_MSG,
                          BLU_FRAME_MSGNO_UNUSED, BLU_FRAME_IGNORE_ANSNO,
                          BLU_FRAME_COMPLETE, buffer, size);
                il -> pl_sent++;
            } else
                bp_log (pi -> channel -> conn, LOG_PROF, 5,
                        "%s start_confirmation: out of memory",
                        pro_name (po -> uri));
            break;
    }
}

static void
pro_close_indication (PROFILE_INSTANCE *pi,
                      DIAGNOSTIC       *request,
                      char              origin,
                      char              scope) {
    if (pro_debug) {
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

    if (pro_debug) {
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

            lib_free (il);
        }
    }
}

static void
pro_tuning_reset_indication (PROFILE_INSTANCE  *pi) {
    if (pro_debug) {
        fprintf (stderr, "%s tuning_reset_indication\n",
                 pro_name (pi -> channel -> profile_registration -> uri));
        fflush (stderr);
    }
}

static void
pro_tuning_reset_confirmation (PROFILE_INSTANCE        *pi,
                               char                     status) {
    if (pro_debug) {
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
                         size;
    char                *buffer,
                        *payload,
                        *response;
    FRAME               *f;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    result = SASL_ALREADY_DONE;

    if (!(f = bpc_query_message (pi -> channel, BLU_QUERY_ANYTYPE,
                                 BLU_QUERY_ANYMSG, BLU_QUERY_ANYANS)))
        return;

    if (pro_debug) {
        fprintf (stderr,
                 "%s message_available: type=%c number=%ld answer=%ld size=%ld\n",
                 pro_name (pi -> channel -> profile_registration -> uri),
                 f -> msg_type, f -> message_number, f -> answer_number,
                 f -> size);
        fprintf (stderr, f -> size > 150 ? "%-150.150s...\n" : "%s\n",
                 f -> payload);
        fflush (stderr);
    }

    size = bpc_frame_aggregate (pi -> channel, f, &buffer);
    if (buffer == NULL) {
        bp_log (pi -> channel -> conn, LOG_PROF, 5,
                "%s message_available: out of memory aggregating message",
                 pro_name (pi -> channel -> profile_registration -> uri));
        bpc_frame_destroy (pi -> channel, f);
        return;
    }

    switch (il -> pl_task) {
        case PRO_CLIENT_ANON:
            break;

        case PRO_CLIENT_OTP:
            if (il -> pl_sent != 2)
                break;

            if (strstr (buffer, "<error") != NULL)
                sasl_error (pi, buffer, size);
            else {
                parse_blob (buffer, &il -> pl_sdb);
                if (!strcmp (il -> pl_sdb.parse_blob_status, "complete"))
                    sasl_set_local_success (pi);
                else
                    sasl_set_local_failure (pi, NULL);
            }
            break;

        case PRO_SERVER_ANON:
            if (il -> pl_sent != 0)
                break;

            if ((result = sasl_anonymous_server_guts (buffer, &il -> pl_sdb))
                    == SASL_COMPLETE)
                config_set (appconfig, SASL_REMOTE_USERNAME,
                            il -> pl_sdb.parse_blob_data);
            break;

        case PRO_SERVER_OTP:
            switch (il -> pl_sent) {
                case 0:
                    result = sasl_otp_server_guts_1 (buffer, &il -> pl_sdb);
                    break;

                case 1:
                    if ((result = sasl_otp_server_guts_2 (buffer, &il -> pl_sdb))
                            == SASL_COMPLETE) {
                        config_set (appconfig, SASL_REMOTE_USERNAME,
                                    il -> pl_sdb.authenID);
                            config_set (appconfig, SASL_REMOTE_TARGET,
                                        (il -> pl_sdb.authorID[0] != '\0')
                                            ? il -> pl_sdb.authorID
                                            : il -> pl_sdb.authenID);
                    }
                    break;
            }
            break;
    }

    bpc_buffer_destroy (pi -> channel, buffer);

    switch (il -> pl_task) {
        case PRO_CLIENT_ANON:
        case PRO_CLIENT_OTP:
            response = NULL;
            break;

        case PRO_SERVER_ANON:
        case PRO_SERVER_OTP:
            response = sasl_response (pi, result);
            break;
    }

    if (f -> msg_type == BLU_FRAME_TYPE_MSG) {
        payload = response ? response
                           : "<error code='521'>Client-side only</error>";
        size = strlen (payload) + sizeof DEFAULT_CONTENT_TYPE - 1;

        if ((buffer = bpc_buffer_allocate(pi -> channel, size)) != NULL) {
            sprintf (buffer, "%s%s", DEFAULT_CONTENT_TYPE, payload);

            bpc_send (pi -> channel,
                      (payload[1] == 'e')
                          ? BLU_FRAME_TYPE_ERR : BLU_FRAME_TYPE_RPY,
                      f -> message_number, f -> answer_number,
                      BLU_FRAME_COMPLETE, buffer, size);
            il -> pl_sent++;
        } else
            bp_log (pi -> channel -> conn, LOG_PROF, 5,
                    "%s message: out of memory sending response",
                    pro_name (pi -> channel -> profile_registration -> uri));
    }

    bpc_frame_destroy (pi -> channel, f);
}

static void
pro_window_full (PROFILE_INSTANCE *pi) {
    DIAGNOSTIC    *d;

    if (pro_debug) {
        fprintf (stderr, "%s window_full\n",
                 pro_name (pi -> channel -> profile_registration -> uri));
        fflush (stderr);
    }

    bp_log (pi -> channel -> conn, LOG_PROF, 5, "%s channel window full!?!",
            pro_name (pi -> channel -> profile_registration -> uri));
    if (!(d = bpc_close_request (pi -> channel, BLU_CHAN0_MSGNO_DEFAULT, 550,
                                 NULL, "stop misbehaving", NULL, NULL))) {
        bp_log (pi -> channel -> conn, LOG_PROF, 4,
                "unable to close SASL channel: [%d] %s", d -> code,
                d -> message);

        bp_diagnostic_destroy (pi -> channel -> conn, d);
    }    
}


/*    helper functions */

static void
sasl_error (PROFILE_INSTANCE       *pi,
            char                   *data,
            int                     size) {
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

static char *
sasl_response (PROFILE_INSTANCE       *pi,
               int                     result) {
    char                *response;
    struct sasl_data_block
                        *sdb = &(((PRO_LOCALDATA *) pi -> user_ptr1) -> pl_sdb);

    switch (result) {
        case SASL_COMPLETE:
            response = "<blob status='complete' />";
            sasl_set_remote_success (pi);
            break;

        case SASL_CONTINUE:
            response = sdb -> payload;
            break;

        case SASL_ABORTED:
            response = "<blob status='complete' />";
            sasl_set_remote_failure (pi, 550, "Aborted by client");
            break;

        case SASL_ERROR_BLOB:
            response = "<error code='501'>Invalid blob format</error>";
            sasl_set_remote_failure (pi, 501, "Invalid blob format");
            break;

        case SASL_ERROR_TRACE:
            response = "<error code='551'>Invalid trace information</error>";
            sasl_set_remote_failure (pi, 551, "Invalid trace information");
            break;

        case SASL_ERROR_PROXY:
            response = "<error code='551'>Proxying not supported</error>";
            sasl_set_remote_failure (pi, 551, "Proxying not supported");
            break;

        case SASL_ERROR_USERNAME:
            response = "<error code='535'>Invalid user name</error>";
            sasl_set_remote_failure (pi, 535, "Invalid user name");
            break;

        case SASL_ERROR_USERBUSY:
            response = "<error code='450'>Database busy</error>";
            sasl_set_remote_failure (pi, 450, "Database busy");
            break;

        case SASL_ERROR_SEQUENCE:
            response = "<error code='534'>Sequence exhausted</error>";
            sasl_set_remote_failure (pi, 534, "Sequence exhausted");
            break;

        case SASL_ERROR_HEXONLY:
            response = "<error code='554'>Expecting hex: response</error>";
            sasl_set_remote_failure (pi, 554, "Expecting hex: response");
            break;

        case SASL_ERROR_BADHASH:
            response = "<error code='535'>Invalid user name</error>";
            sasl_set_remote_failure (pi, 535, "Bad response");
            break;

        default:
            response = "<error code='504'>Internal error</error>";
            sasl_set_remote_failure (pi, 504, "Internal error");
            break;
    }

    return response;
}

static void
sasl_set_local_success (PROFILE_INSTANCE       *pi) {
    int                  code;
    char                *reason;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    if (pro_debug) {
        fprintf (stderr, "sasl_set_local_success\n");
        fflush (stderr);
    }

    if (config_test_and_set (appconfig, SASL_LOCAL_CODE, "200") != CONFIG_OK) {
        code = atoi (config_get (appconfig, SASL_LOCAL_CODE));
        reason = config_get (appconfig, SASL_LOCAL_REASON);
    } else {
        code = 200;
        config_set (appconfig, SASL_LOCAL_REASON, reason = "login successful");
    }

    il -> pl_status = bp_diagnostic_new (pi -> channel -> conn, code, NULL,
                                         "%s", reason);
}

static void
sasl_set_local_failure (PROFILE_INSTANCE       *pi,
                        DIAGNOSTIC             *d) {
    int                  code;
    char                 buffer[BUFSIZ],
                        *reason;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    if (d) {
        code = d -> code;
        reason = d -> message ? d -> message : "";
    } else if (!il -> pl_sdb.parse_blob_status[0])
        code = 501, reason = "unable to parse blob from server";
    else if (!strcmp (il -> pl_sdb.parse_blob_status, "abort"))
        code = 450, reason = "server returned 'abort' status";
    else if (!strcmp (il -> pl_sdb.parse_blob_status, "continue"))
        code = 300, reason = "server returned 'continue' status";
    else {
        code = 555;
        sprintf (reason = buffer, "server returned '%s' status",
                 il -> pl_sdb.parse_blob_status);
    }

    if (pro_debug) {
        fprintf (stderr, "sasl_set_local_failure: code=%d reason=\"%s\"\n",
                 code, reason);
        fflush (stderr);
    }

    config_set (appconfig, SASL_LOCAL_REASON, reason);

    sprintf (buffer, "%d", code);
    config_set (appconfig, SASL_LOCAL_CODE, buffer);

    il -> pl_status = bp_diagnostic_new (pi -> channel -> conn, code, NULL,
                                         "%s", reason);
}

static void
sasl_set_remote_success (PROFILE_INSTANCE       *pi) {
    char    *cp;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    if (pro_debug) {
        fprintf (stderr, "sasl_set_remote_success\n");
        fflush (stderr);
    }

    if (config_test_and_set (appconfig, SASL_REMOTE_CODE, "200")
            == CONFIG_OK) {
        config_set (appconfig, SASL_REMOTE_REASON, "login successful");

        cp = config_get (appconfig, SASL_REMOTE_MECHANISM);
        bp_log (pi -> channel -> conn, LOG_PROF, 2, "sasl 200 %s %s", cp,
                config_get (appconfig, SASL_REMOTE_USERNAME));
    }
}

static void
sasl_set_remote_failure (PROFILE_INSTANCE       *pi,
                         int                     code,
                         char                   *reason) {
    char        buffer[BUFSIZ];
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    if (pro_debug) {
        fprintf (stderr, "sasl_set_remote_failure: code=%d reason=\"%s\"\n",
                 code, reason);
        fflush (stderr);
    }

    sprintf (buffer, "%d", code);
    config_set (appconfig, SASL_REMOTE_CODE, buffer);
    config_set (appconfig, SASL_REMOTE_REASON, reason);

    bp_log (pi -> channel -> conn, LOG_PROF, 3, "sasl %d %s", code, reason);
}


/*
 * Additional profile-related functions.
 */

DIAGNOSTIC *
sasl_login (BP_CONNECTION             *bp,
            char                      *serverName) {
    char                *cp,
                        *mechanism;
    DIAGNOSTIC          *d;
    PRO_LOCALDATA       *il;
    PROFILE              ps,
                        *p = &ps;
    struct configobj    *appconfig = bp_get_config (bp);

    if (((cp = config_get (appconfig, SASL_REMOTE_CODE)) != NULL)
            && (*cp == '2'))
        return bp_diagnostic_new (bp, 520, NULL,
                                  "already tuned for authentication");

    if (!(mechanism = config_get (appconfig,
                                  SASL_LOCAL_MECHANISM)))
        return bp_diagnostic_new (bp, 501, NULL, "mechanism undefined");

    if (!(il = (PRO_LOCALDATA *) lib_malloc (sizeof *il)))
        return bp_diagnostic_new (bp, 421, NULL, "out of memory");

    memset (il, 0, sizeof *il);
    il -> pl_flags = PRO_INITIATOR | PRO_STARTING | PRO_CLOSED;
    il -> pl_sent = 1;

    memset (p, 0, sizeof *p);

    if (!strcasecmp (mechanism, "anonymous")) {
        il -> pl_task = PRO_CLIENT_ANON;
        if (!(p -> uri = config_get (appconfig, SASL_ANONYMOUS_URI)))
            p -> uri = PRO_ANONYMOUS_URI;
        p -> piggyback =
              sasl_anonymous_client_guts (config_get (appconfig,
                                                      SASL_LOCAL_TRACEINFO),
                                          &il -> pl_sdb);
    } else if (!strcasecmp (mechanism, "otp")) {
        char    *authenID,
                *authorID;

        il -> pl_task = PRO_CLIENT_OTP;
        if (!(p -> uri = config_get (appconfig, SASL_OTP_URI)))
            p -> uri = PRO_OTP_URI;
        if (!(authenID = config_get (appconfig, SASL_LOCAL_USERNAME))) {
            lib_free (il);
            return bp_diagnostic_new (bp, 501, NULL,
                                      "authentication ID undefined");
        }
        authorID = config_get (appconfig, SASL_LOCAL_TARGET);

        p -> piggyback = sasl_otp_client_guts_1 (authenID, authorID,
                                                 &il -> pl_sdb);
    } else {
        lib_free (il);
        return bp_diagnostic_new (bp, 501, NULL, "mechanism unknown: %s",
                                  mechanism);
    }
    p -> piggyback_length = strlen (p -> piggyback);

    config_delete (appconfig, SASL_LOCAL_CODE);
    config_delete (appconfig, SASL_LOCAL_REASON);
    config_set (appconfig, SASL_LOCAL_MECHANISM, mechanism);

    if ((d = bp_start_request (bp, BLU_CHAN0_CHANO_DEFAULT,
                               BLU_CHAN0_MSGNO_DEFAULT, p, serverName, NULL,
                               (void *) il)) != NULL)
        goto out;

    while ((il -> pl_status == NULL)
               && ((il -> pl_flags & (PRO_STARTING|PRO_CLOSED)) != PRO_CLOSED)
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
    if (!(il -> pl_flags & PRO_CLOSED))
        il -> pl_channel -> profile -> user_ptr1 = NULL;

    lib_free (il);

    return d;
}


/*
 * Well-known shared library entry point.
 */

PROFILE_REGISTRATION *sasl_profiles_Init (struct configobj *appconfig) {
    char                *cp;
    PROFILE_REGISTRATION anons,
                        *anon = &anons,
                         otps,
                        *otp = &otps;

    pro_debug = 0;
    if ((cp = config_get (appconfig, SASL_FAMILY_DEBUG)) != NULL)
        pro_debug = atoi (cp);

    memset (anon, 0, sizeof *anon);

    if (!(anon -> uri = config_get (appconfig, SASL_ANONYMOUS_URI)))
        anon -> uri = PRO_ANONYMOUS_URI;

    if (!(anon -> initiator_modes = config_get (appconfig,
                                                SASL_ANONYMOUS_IMODES)))
        anon -> initiator_modes = "plaintext,encrypted";
    if (!(anon -> listener_modes = config_get (appconfig,
                                                SASL_ANONYMOUS_LMODES)))
        anon -> listener_modes = "plaintext,encrypted";

    anon -> full_messages = 1;
    anon -> thread_per_channel = 0;

    anon -> proreg_connection_init = pro_connection_init;
    anon -> proreg_connection_fin  = pro_connection_fin;
    anon -> proreg_session_init = pro_session_init;
    anon -> proreg_session_fin  = pro_session_fin;
    anon -> proreg_start_indication = pro_start_indication;
    anon -> proreg_start_confirmation = pro_start_confirmation;
    anon -> proreg_close_indication   = pro_close_indication;
    anon -> proreg_close_confirmation = pro_close_confirmation;
    anon -> proreg_tuning_reset_indication   = pro_tuning_reset_indication;
    anon -> proreg_tuning_reset_confirmation = pro_tuning_reset_confirmation;
    anon -> proreg_message_available = pro_message_available;
    anon -> proreg_window_full       = pro_window_full;

    memcpy (otp, anon, sizeof *otp);

    if (!(otp -> uri = config_get (appconfig, SASL_OTP_URI)))
        otp -> uri = PRO_OTP_URI;

    if (!(otp -> initiator_modes = config_get (appconfig, SASL_OTP_IMODES)))
        otp -> initiator_modes = "plaintext,encrypted";
    if (!(otp -> listener_modes = config_get (appconfig, SASL_OTP_LMODES)))
        otp -> listener_modes = "plaintext,encrypted";

    if ((anon = bp_profile_registration_clone (NULL, anon)) != NULL)
        anon -> next = bp_profile_registration_clone (NULL, otp);

    return anon;
}
