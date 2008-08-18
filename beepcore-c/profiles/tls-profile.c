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
 * $Id: tls-profile.c,v 1.44 2002/04/27 04:40:22 huston Exp $
 */


/*    includes */

#ifndef WIN32
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#else
#define sem_t HANDLE
#endif
#include "tls-profile.h"
#include "../threaded_os/wrapper/bp_notify.h"
#include "../threaded_os/utility/logutil.h"
#include "../threaded_os/utility/workthread.h"
#include "../utility/bp_slist_utility.h"
#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../threaded_os/transport/bp_fiostate.h"
#include "../threaded_os/transport/bp_fpollmgr.h"


/*    defines and typedefs */

#define PRO_TLS_URI             "http://iana.org/beep/TLS"


typedef struct pro_localdata {
    int                  pl_flags;
#define PRO_INITIATOR   (1<<0)
#define PRO_STARTING    (1<<1)
#define PRO_CLOSED      (1<<2)

    CHANNEL_INSTANCE    *pl_channel;
    DIAGNOSTIC          *pl_status;

    SSL_CTX             *pl_ctx;
}  PRO_LOCALDATA;


/*    statics */

static int       pro_debug;
static WORKQUEUE* notifyqueue = NULL;

static void     tls_tuning_reset_server_handshake (PROFILE_INSTANCE *pi);
static void     tls_tuning_reset_notify (void* data);
static void     tls_tuning_reset_client_handshake (PROFILE_INSTANCE *pi);

static void     tls_set_success (PROFILE_INSTANCE *pi,
                                 SSL              *ssl);
static void     tls_set_failure (PROFILE_INSTANCE *pi,
                                 int               error);

static int      tls_reader (void *handle,
                            char *buffer,
                            int  size);
static int      tls_writer (void *handle,
                            char *buffer,
                            int  size);
static int      tls_destroy (void *handle);


/*    profile methods */

static char *
pro_connection_init (PROFILE_REGISTRATION *pr,
                     BP_CONNECTION        *bp) {
    if (pro_debug) {
        fprintf (stderr, "TLS connection_init\n");
        fflush (stderr);
    }

    return NULL;
}

static char *
pro_session_init (PROFILE_REGISTRATION *pr,
                  BP_CONNECTION        *bp) {
    if (pro_debug) {
        fprintf (stderr, "TLS session_init: role='%c' mode=\"%s\"\n",
                 bp -> role, bp -> mode);
        fflush (stderr);
    }

    return NULL;
}

static char *
pro_session_fin (PROFILE_REGISTRATION *pr,
                 BP_CONNECTION        *bp) {
    if (pro_debug) {
        fprintf (stderr, "TLS session_fin\n");
        fflush (stderr);
    }

    return NULL;
}

static char *
pro_connection_fin (PROFILE_REGISTRATION *pr,
                    BP_CONNECTION        *bp) {
    if (pro_debug) {
        fprintf (stderr, "TLS connection_fin\n");
        fflush (stderr);
    }

    return NULL;
}

static void
pro_start_indication (PROFILE_INSTANCE *pi,
                      PROFILE          *po) {
    char                 certbuf[BUFSIZ],
                        *certfile,
                        *cp,
                        *dir,
                         keybuf[BUFSIZ],
                        *keyfile,
                        *serverName;
    DIAGNOSTIC           ds,
                        *d = &ds;
    PROFILE              ps,
                        *p = &ps;
    PRO_LOCALDATA       *il;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);
    struct cfgsearchobj *cs;

    if (!(serverName = bp_server_name (pi -> channel -> conn)))
        serverName = pi -> channel -> inbound -> server_name;
    if (pro_debug) {
        fprintf (stderr,
                 "TLS start_indication: servername=\"%s\" piggyback=\"%s\"\n",
                 serverName ? serverName : "<NULL>",
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

    certfile = keyfile = NULL;
    if ((dir = config_get (appconfig, TLS_CERTDIR))
            && !(strstr (serverName, "/"))
            && !(strstr (serverName, ".."))) {
        sprintf (certbuf, "%s/%s", dir, serverName);
#ifndef WIN32
        if (access (certbuf, R_OK) >= 0) {
            certfile = keyfile = certbuf;
            if ((dir = config_get (appconfig, TLS_KEYDIR)) != NULL) {
                sprintf (keybuf, "%s/%s", dir, serverName);
                if (access (keybuf, R_OK) >= 0)
                    keyfile = keybuf;
                else
                    certfile = keyfile = NULL;
            }
        }
#else
        certfile = keyfile = certbuf;
        if ((dir = config_get (appconfig, TLS_KEYDIR)) != NULL) {
            sprintf (keybuf, "%s/%s", dir, serverName);
            keyfile = keybuf;
        }
#endif
    }

    memset (il, 0, sizeof *il);
    if (!(il -> pl_ctx = SSL_CTX_new (SSLv23_server_method ()))) {
        if (pro_debug)
            ERR_print_errors_fp (stderr);
        d -> message = "unable to create server context";
    } else if (!certfile
                   && !(certfile = config_get (appconfig, TLS_CERTFILE))) {
        d -> message = "TLS_CERTFILE not present in configuration";
    } else if (SSL_CTX_use_certificate_file (il -> pl_ctx, certfile,
                                             SSL_FILETYPE_PEM) <= 0) {
        if (pro_debug)
            ERR_print_errors_fp (stderr);
        d -> message = "unable to use certificate file";
    } else if (!keyfile && !(keyfile = config_get (appconfig, TLS_KEYFILE))) {
        d -> message = "TLS_KEYFILE not present in configuration";
    } else if (SSL_CTX_use_PrivateKey_file (il -> pl_ctx, keyfile,
                                            SSL_FILETYPE_PEM) <= 0) {
        if (pro_debug)
            ERR_print_errors_fp (stderr);
        d -> message =  "unable to use private key file";
    } else if (!SSL_CTX_check_private_key (il -> pl_ctx)) {
        d -> message = "private key does not match certificate's public key";
    } else if (po -> piggyback_length == 0)
        d = NULL;
    else if (strstr (po -> piggyback, "<ready")) {
        bp_slist *list;
        CHANNEL_INSTANCE *inst;

        for (list = pi -> channel -> conn -> channel_instance;
                 list;
                 list = list -> next)
        {
            if ((inst = list -> data) -> channel_number == 0) {
                bpc_tuning_reset_request (inst);
                break;
            }
        }

        p -> piggyback_length = strlen (p -> piggyback = "<proceed />");

#if 0
        err = workqueue_add_item(notifyqueue, tls_tuning_reset_notify, pi);
        if (err == 0) {
            fiostate_block_read (GET_WRAPPER(pi->channel->conn) -> iostate);
            d = NULL;
        } else {
            d -> message = "unable to wait for channels to become quiescent";
        }
#else
        fiostate_block_read (GET_WRAPPER(pi->channel->conn) -> iostate);
        d = NULL;
#endif
    } else
        d = bp_diagnostic_new (pi -> channel -> conn, 504, NULL,
                               "expecting ready element in piggyback");

    if (d && d -> message) {
        lib_free (il);
    } else {
        pi -> user_ptr1 = il;
        cs = config_search_init (appconfig, PRIVACY_PREFIX, "");
        for (cp = config_search_string_firstkey (cs);
                cp;
                cp = config_search_string_nextkey (cs))
            config_delete (appconfig, cp);
        config_search_fin (&cs);
    }
    
    bpc_start_response (pi -> channel, p, d);
#if 1
    workqueue_add_item(notifyqueue, tls_tuning_reset_notify, pi);
#endif

    if ((d != NULL) && (d != &ds))
        bp_diagnostic_destroy (pi -> channel -> conn, d);
}

static void
pro_start_confirmation (void             *clientData,
                        PROFILE_INSTANCE *pi,
                        PROFILE          *po) {
    PRO_LOCALDATA *il = (PRO_LOCALDATA *) clientData;

    if (pro_debug) {
        fprintf (stderr, "TLS start_confirmation: piggyback=\"%s\"\n",
                 po -> piggyback ? po -> piggyback : "<NULL>");
        fflush (stderr);
    }
               
    pi -> user_ptr1 = il;

    il -> pl_flags &= ~(PRO_STARTING | PRO_CLOSED);
    il -> pl_channel = pi -> channel;

    if ((po -> piggyback_length > 0) && strstr (po -> piggyback, "<proceed"))
#if 1
        tls_tuning_reset_client_handshake (pi);
#else
        workqueue_add_item(notifyqueue, tls_tuning_reset_client_handshake, pi);
#endif
    else
      il -> pl_status = bp_diagnostic_new (pi -> channel -> conn, 504, NULL,
                                           "expecting proceed element");
}

static void
pro_close_indication (PROFILE_INSTANCE *pi,
                      DIAGNOSTIC       *request,
                      char              origin,
                      char              scope) {
    if (pro_debug) {
        fprintf (stderr,
                 "TLS close_indication: request=[%d] \"%s\" origin=%c scope=%c\n",
                 request -> code, request -> message ? request -> message
                                                     : "<NULL>",
                 origin, scope);
        fflush (stderr);
    }
               
    bpc_close_response (pi -> channel, NULL);
}

static void
pro_close_confirmation(PROFILE_INSTANCE *pi,
                       char              status,
                       DIAGNOSTIC       *error,
                       char              origin,
                       char              scope) {
    PRO_LOCALDATA *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (pro_debug) {
        if (error)
            fprintf (stderr,
                     "TLS close_confirmation: status=%c error=[%d] \"%s\" origin=%c scope=%c\n",
                     status, error -> code, error -> message ? error -> message
                                                             : "<NULL>",
                     origin, scope);
        else 
            fprintf (stderr,
                     "TLS close_confirmation: status=%c no error origin=%c scope=%c\n",
                     status, origin, scope);
        fflush (stderr);
    }

    if (status != PRO_ACTION_SUCCESS)
        return;

    if (il) {
        if (il -> pl_flags & PRO_INITIATOR)
            il -> pl_flags |= PRO_CLOSED;
        else {
            pi -> user_ptr1 = NULL;

            SSL_CTX_free (il -> pl_ctx);
            lib_free (il);
        }
    }
}

static void
pro_tuning_reset_indication (PROFILE_INSTANCE *pi) {
    if (pro_debug) {
        fprintf (stderr, "TLS tuning_reset_indication\n");
        fflush (stderr);
    }
}

static void
pro_tuning_reset_confirmation (PROFILE_INSTANCE *pi,
                               char              status) {
    if (pro_debug) {
        fprintf (stderr, "TLS tuning_reset_confirmation: status=%c\n", status);
        fflush (stderr);
    }

    pro_close_confirmation (pi, status, NULL, PRO_ACTION_LOCAL,
                            PRO_ACTION_SESSION);
}

static void
pro_message_available(PROFILE_INSTANCE *pi) {
    int            type;
    char          *buffer;
    FRAME         *f;
    PRO_LOCALDATA *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    
    if (!(f = bpc_query_message (pi -> channel, BLU_QUERY_ANYTYPE,
                                  BLU_QUERY_ANYMSG, BLU_QUERY_ANYANS)))
        return;

    if (pro_debug) {
        fprintf (stderr,
                 "TLS message_available: type=%c number=%ld answer=%ld size=%ld\n",
                 f -> msg_type, f -> message_number, f -> answer_number,
                 f -> size);
        fprintf (stderr, f -> size > 75 ? "%-75.75s...\n" : "%s\n",
                 f -> payload);
        fflush (stderr);
    }

    switch (f -> msg_type) {
        case BLU_FRAME_TYPE_MSG:
            type = BLU_FRAME_TYPE_ERR;
            if (il -> pl_flags & PRO_INITIATOR)
                buffer = bpc_error_allocate (pi -> channel, 521, NULL,
                                             "client-side only");
            else if (!strstr (f -> payload, "<ready")) {
                buffer = bpc_error_allocate (pi -> channel, 504, NULL,
                                              "expecting ready element");
            } else if ((buffer = bpc_buffer_allocate (pi -> channel,
                                                       sizeof "<proceed />"
                                                           - 1)) != NULL) {
                type = BLU_FRAME_TYPE_RPY;
                strcpy (buffer, "<proceed />");
                bpc_tuning_reset_request (pi -> channel);
                fiostate_block_read(GET_WRAPPER(pi->channel->conn) -> iostate);
            }
            if (buffer != NULL) {
                bpc_send (pi -> channel, type, f -> message_number,
                          BLU_FRAME_IGNORE_ANSNO, BLU_FRAME_COMPLETE, buffer,
                          strlen (buffer));
                if (type == BLU_FRAME_TYPE_RPY) {
                    bpc_frame_destroy (pi -> channel, f);
                    tls_tuning_reset_server_handshake (pi);
                    return;
                }
            } else
                bp_log (pi -> channel -> conn, LOG_PROF, 5,
                        "TLS message: out of memory allocating buffer/error");
            break;

        case BLU_FRAME_TYPE_RPY:
            if (strstr (f -> payload, "<proceed")) {
                bpc_frame_destroy (pi -> channel, f);
                tls_tuning_reset_client_handshake (pi);
                return;
            }
            /* and fall... */
        case BLU_FRAME_TYPE_ANS:
        case BLU_FRAME_TYPE_NUL:
        case BLU_FRAME_TYPE_ERR:
            bpc_tuning_reset_complete (pi -> channel, PRO_ACTION_FAILURE,
                                       NULL);
            break;
    }

    bpc_frame_destroy (pi -> channel, f);
}

static void
pro_window_full (PROFILE_INSTANCE *pi) {
    DIAGNOSTIC    *d;

    if (pro_debug) {
        fprintf (stderr, "TLS window_full\n");
        fflush (stderr);
    }

    bp_log (pi -> channel -> conn, LOG_PROF, 5, "TLS channel window full!?!");
    if (!(d = bpc_close_request (pi -> channel, BLU_CHAN0_MSGNO_DEFAULT, 550,
                                 NULL, "stop misbehaving", NULL, NULL))) {
        bp_log (pi -> channel -> conn, LOG_PROF, 4,
                "unable to close TLS channel: [%d] %s", d -> code,
                d -> message);
        bp_diagnostic_destroy (pi -> channel -> conn, d);
    }    
}


/*    helper functions */

static void
tls_tuning_reset_server_handshake (PROFILE_INSTANCE *pi) {
    int                  error;
    char                *cp;
    BIO                 *bio;
    SSL                 *ssl;
    BP_CONNECTION       *bp = pi -> channel -> conn;
    WRAPPER             *w = GET_WRAPPER (bp);
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    struct configobj    *appconfig = bp_get_config (bp);
    struct cfgsearchobj *cs;

    if (pro_debug) {
        fprintf (stderr, "TLS server_handshake\n");
        fflush (stderr);
    }

    fiostate_unblock_write (w -> iostate);
    fiostate_block_read (w -> iostate);

    bpc_wait_state_quiescent (pi -> channel);

    fiostate_block_read (w -> iostate);
    fiostate_block_write (w -> iostate);

    ssl = SSL_new (il -> pl_ctx);
    bio = BIO_new (BIO_s_socket ());
    BIO_set_fd (bio, w -> iostate -> socket, BIO_NOCLOSE);
    BIO_set_nbio (bio, 1);
    SSL_set_bio (ssl, bio, bio);

    do {
        error = SSL_get_error (ssl, SSL_accept (ssl));
    } while ((error == SSL_ERROR_WANT_READ)
                 || (error == SSL_ERROR_WANT_WRITE));

    cs = config_search_init (appconfig, SASL_REMOTE_PREFIX, "");
    for (cp = config_search_string_firstkey (cs);
             cp;
             cp = config_search_string_nextkey (cs))
        config_delete (appconfig, cp);
    config_search_fin (&cs);

    if (error != SSL_ERROR_NONE) {
        tls_set_failure (pi, error);
        SSL_free (ssl);
        bpc_tuning_reset_complete (pi -> channel, PRO_ACTION_SUCCESS,
                                   "plaintext");
    } else {
        tls_set_success (pi, ssl);
        bp_new_reader_writer (w -> iostate, tls_reader, ssl, tls_writer,
                              ssl, 4096, NULL /*tls_destroy*/, NULL);
        bpc_tuning_reset_complete (pi -> channel, PRO_ACTION_SUCCESS,
                                    "encrypted");
    }
}

static void
tls_tuning_reset_notify (void* data) {
    PROFILE_INSTANCE *pi = (PROFILE_INSTANCE*) data;

    if (pi -> channel -> conn -> status == INST_STATUS_EXIT)
        return;

    bp_wait_channel_0_state_quiescent (pi -> channel -> conn);

    tls_tuning_reset_server_handshake (pi);
}

static void
tls_tuning_reset_client_handshake (PROFILE_INSTANCE *pi) {
    int                  error;
    char                *cp;
    BIO                 *bio;
    SSL                 *ssl;
    BP_CONNECTION       *bp = pi -> channel -> conn;
    WRAPPER             *w = GET_WRAPPER (bp);
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    struct configobj    *appconfig = bp_get_config (bp);
    struct cfgsearchobj *cs;

    if (pro_debug) {
        fprintf (stderr, "TLS client_handshake\n");
        fflush (stderr);
    }

    fiostate_block_read(w->iostate);
    fiostate_block_write(w->iostate);
    ssl = SSL_new (il -> pl_ctx);
    bio = BIO_new (BIO_s_socket ());
    BIO_set_fd (bio, w -> iostate -> socket, BIO_NOCLOSE);
    BIO_set_nbio (bio, 1);
    SSL_set_bio (ssl, bio, bio);

    do
    {
        error = SSL_get_error (ssl, SSL_connect (ssl));
    } while ((error == SSL_ERROR_WANT_READ)
                 || (error == SSL_ERROR_WANT_WRITE));

    il -> pl_flags |= PRO_CLOSED;
    cs = config_search_init (appconfig, SASL_REMOTE_PREFIX, "");
    for (cp = config_search_string_firstkey (cs);
             cp;
             cp = config_search_string_nextkey (cs))
        config_delete (appconfig, cp);
    config_search_fin (&cs);

    if (error != SSL_ERROR_NONE) {
        tls_set_failure (pi, error);
        SSL_free (ssl);
        bpc_tuning_reset_complete (pi -> channel, PRO_ACTION_SUCCESS,
                                   "plaintext"); 
    } else {
        tls_set_success (pi, ssl);
        bp_new_reader_writer(w -> iostate, tls_reader, ssl, tls_writer,
                             ssl, 4096, NULL /* tls_destroy */, NULL);
        bpc_tuning_reset_complete (pi -> channel, PRO_ACTION_SUCCESS,
                                    "encrypted"); 
    }
}

static void
tls_set_success (PROFILE_INSTANCE *pi,
                 SSL              *ssl) {
    int                  bits;
    char                 buffer[BUFSIZ],
                        *cp;
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;
    SSL_CIPHER          *cipher = SSL_get_current_cipher (ssl);
    X509                *x509;
    struct configobj    *appconfig = bp_get_config (pi -> channel -> conn);

    if (pro_debug) {
        fprintf (stderr, "tls_set_success\n");
        fflush (stderr);
    }

    if ((x509 = SSL_get_peer_certificate (ssl)) != NULL) {
        cp = X509_NAME_oneline (X509_get_subject_name (x509), NULL, 0);
        config_set (appconfig, PRIVACY_REMOTE_SUBJECTNAME, cp);
#if 0
        /** @todo fix this */
        free (cp);
#endif
    }
    config_set (appconfig, PRIVACY_CIPHER_NAME,
                (char *) SSL_CIPHER_get_name (cipher));
    config_set (appconfig, PRIVACY_CIPHER_PROTOCOL,
                SSL_CIPHER_get_version (cipher));
    sprintf (buffer, "%d", SSL_CIPHER_get_bits (cipher, &bits));
    config_set (appconfig, PRIVACY_CIPHER_SBITS, buffer);

    if (il -> pl_flags & PRO_INITIATOR)
        il -> pl_status = bp_diagnostic_new (pi -> channel -> conn, 200, NULL,
                                             "tuning successful");
}

static void
tls_set_failure (PROFILE_INSTANCE *pi,
                 int               error) {
    PRO_LOCALDATA       *il = (PRO_LOCALDATA *) pi -> user_ptr1;

    if (il -> pl_flags & PRO_INITIATOR)
        il -> pl_status = bp_diagnostic_new (pi -> channel -> conn, 550, NULL,
                                             "openssl returned %d", error);
    else
        log_line (LOG_PROF, 5, "openssl returned %d", error);
}

static int
tls_reader (void *handle,
            char *buffer,
            int   size) {
    SSL *ssl = (SSL *) handle;

    return SSL_read (ssl, buffer, size);
}

static int
tls_writer (void *handle,
            char *buffer,
            int   size) {
    SSL *ssl = (SSL *) handle;

    return SSL_write (ssl, buffer, size);
}

static int
tls_destroy (void *handle) {
    SSL *ssl = (SSL *) handle;

    if (ssl)
        SSL_free (ssl);

    return 0;
}


/*
 * Additional profile-related functions.
 */

DIAGNOSTIC *
tls_privatize (BP_CONNECTION           *bp,
               char                    *serverName) {
    char                *cp;
    bp_slist            *list;
    CHANNEL_INSTANCE    *ci;
    DIAGNOSTIC          *d;
    PROFILE              ps,
                        *p = &ps;
    PRO_LOCALDATA       *il;
    struct configobj    *appconfig = bp_get_config (bp);
    struct cfgsearchobj *cs;

    if (((cp = config_get (appconfig, SASL_REMOTE_CODE)) != NULL)
            && (*cp == '2'))
        return bp_diagnostic_new (bp, 520, NULL,
                                  "already tuned for authentication");

    if (!(il = (PRO_LOCALDATA *) lib_malloc (sizeof *il)))
        return bp_diagnostic_new (bp, 421, NULL, "out of memory");

    memset (il, 0, sizeof *il);
    il -> pl_flags = PRO_INITIATOR | PRO_STARTING | PRO_CLOSED;
    if (!(il -> pl_ctx = SSL_CTX_new (TLSv1_client_method ()))) {
        if (pro_debug)
            ERR_print_errors_fp (stderr);
        lib_free (il);
        
        return bp_diagnostic_new (bp, 421, NULL,
                                  "unable to create client context");
    }

    for (list = bp -> channel_instance; list; list = list -> next)
        if ((ci = list -> data) -> channel_number == 0) {
            bpc_tuning_reset_request (ci);
            break;
        }

    memset (p, 0, sizeof *p);
    p -> uri = PRO_TLS_URI;
    p -> piggyback_length = strlen (p -> piggyback = "<ready />");

    cs = config_search_init (appconfig, PRIVACY_PREFIX, "");
    for (cp = config_search_string_firstkey (cs);
             cp;
             cp = config_search_string_nextkey (cs))
        config_delete (appconfig, cp);
    config_search_fin (&cs);

    if ((d = bp_start_request(bp, BLU_CHAN0_CHANO_DEFAULT,
                              BLU_CHAN0_MSGNO_DEFAULT, p, serverName, NULL,
                              (void *) il)) != NULL)
        goto out;

    while ((il -> pl_status == NULL)
               && ((il -> pl_flags & (PRO_STARTING|PRO_CLOSED)) != PRO_CLOSED)
               && (bp -> status != INST_STATUS_EXIT))
        YIELD ();

    while (bp->status == INST_STATUS_TUNING)
        YIELD();

    if (!il -> pl_status) {
        d = bp_diagnostic_new (bp, 400, NULL,
                               bp -> status != INST_STATUS_EXIT
                                    ? "lost channel during tuning"
                                    : "lost session during tuning");
        goto out;
    }

    if (il -> pl_status -> code >= 400) {
        d = il -> pl_status;
        goto out;
    }
    bp_diagnostic_destroy (bp, il -> pl_status);

    if ((!(d = bp_wait_for_greeting (bp)))
            && (bp -> status == INST_STATUS_EXIT))
        d = bp_diagnostic_new (bp, 400, NULL, "lost session after tuning");

out: ;
    if (!(il -> pl_flags & PRO_CLOSED))
        il -> pl_channel -> profile -> user_ptr1 = NULL;

    SSL_CTX_free (il -> pl_ctx);
    lib_free (il);

    return d;
}


/*
 * Well-known shared library entry point.
 */

PROFILE_REGISTRATION *tls_profile_Init(struct configobj *appconfig) {
    char                *cp;
    PROFILE_REGISTRATION prs,
                        *pr = &prs;
  
    pro_debug = 0;
    if ((cp = config_get (appconfig, TLS_DEBUG)) != NULL)
        pro_debug = atoi (cp);

    memset (pr, 0, sizeof *pr);

    if (!(pr -> uri = config_get (appconfig, TLS_URI)))
        pr -> uri = PRO_TLS_URI;

    pr -> initiator_modes = "plaintext";
    pr -> listener_modes = "plaintext";

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

    SSL_library_init ();
    SSL_load_error_strings ();

    if (!(cp = config_get (appconfig, TLS_CERTFILE))) {
        log_line (LOG_PROF, 6, "TLS_CERTFILE not present in configuration");
        return NULL;
    }
    if (!strstr (cp, ".pem")) {
        log_line (LOG_PROF, 6, "TLS_CERTFILE doesn't end in \".pem\"");
        return NULL;
    }
    if (!(cp = config_get (appconfig, TLS_KEYFILE))) {
        log_line (LOG_PROF, 6, "TLS_KEYFILE not present in configuration");
        return NULL;
    }
    if (!strstr (cp,".pem")) {
        log_line (LOG_PROF, 6, "TLS_KEYFILE doesn't end in \".pem\"");
        return NULL;
    }

    notifyqueue = workqueue_create();
    if (notifyqueue == NULL) {
        log_line (LOG_PROF, 6, "Unable to initalize worker threads");
        return NULL;
    }

    return bp_profile_registration_clone (NULL, pr);
}
