/* profile for SYSLOG reliable RAW */

/*
 * $Id: syslog-raw.h,v 1.7 2002/01/01 00:25:15 mrose Exp $
 */

/*

initiator usage:

   1. pr = sr_Init (appconfig)
      sr_initiator (pr, callback, clientData)

   2. establish session using pr

   3. await invocation of callback (v, code, diagnostic, clientData)

       if v is NULL:
           look at code/diagnostic for reason
           release session, etc.

       otherwise:
            remember v

   4. for 0..I: sr_log (v, msg)

   5. sr_fin (v)

   6. await invocation of callback (v, code, diagnostic, clientData)

      look at code/diagnostic for success/failure


listener usage:

   1. pr = sr_Init (appconfig)
      sr_listener (pr, callback, clientData)

   2. establish session using pr

   3. forever: await invocation of callback (v, code, diagnostic, clientData)

       if v is NULL:
           look at code/diagnostic for reason
           release session, etc.

       if diagnostic is NULL:
           we're now plugged in (may happen more than once)
       
       otherwise:
            give diagnostic (of length code) to logging system

 */

#ifndef SR_PROFILE_H
#define SR_PROFILE_H    1


/* includes */

#include "../threaded_os/utility/bp_config.h"
#include "../threaded_os/wrapper/bp_wrapper.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @name The SYSLOG reliable RAW profile
 **/

//@{

/* typedefs */

/**
 * Prototype for the application callback
 *
 * @param v A profile-provided pointer to be used for future calls to
 *         {@link sr_log sr_log} or {@link sr_fin sr_fin}.
 *         If <b>NULL</b>, then the channel has closed, and the session may
 *         be released.
 *
 * @param code A numeric result code.
 * Consult \URL[Section 8 of RFC 3080]{http://www.beepcore.org/beepcore/docs/rfc3080.jsp#reply-codes}
 * for an (incomplete) list.
 *
 * @param diagnostic A textual message corresopnding to the <i>code</i> parameter.
 *
 * @param clientData The user-supplied pointer provided to either
 *                   {@link sr_initiator sr_initiator} or
 *                   {@link sr_listener sr_listener}.
 **/
typedef void (*sr_callback) (void *v,
                             int code,
                             char *message,
                             void *clientData);


/* keys for configuration package */

/**
 * URI to use for the SYSLOG reliable RAW profile,
 * defaults to <b>"http://iana.org/beep/SYSLOG/RAW"</b>.
 **/
#define SYSLOG_RAW_URI           "beep profiles syslog_raw uri"

/**
 * Initiator modes for the SYSLOG reliable RAW profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define SYSLOG_RAW_IMODES        "beep profiles syslog_raw initiator_modes"

/**
 * Listener modes for the SYSLOG reliable RAW profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define SYSLOG_RAW_LMODES        "beep profiles syslog_raw listener_modes"

/**
 * User-specified window size for the SYSLOG reliable RAW profile.
 **/
#define SYSLOG_RAW_WINDOWSIZE    "beep profiles syslog_raw window_size"

/**
 * Enables debug mode for the SYSLOG reliable RAW profile,
 * defaults to <b>"0"</b>.
 **/
#define SYSLOG_RAW_DEBUG         "beep profiles syslog_raw debug"


/* module entry points */

/**
 * Well-known entry point for the SYSLOG reliable RAW profile.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure used for configuration purposes.
 * <p>
 * Keys:
 * <blockquote><dl>
 * <dt>SYSLOG_RAW_URI</dt>
 * <dd>The registration URI for the profile (optional).</dd>
 *
 * <dt>SYSLOG_RAW_IMODES</dt>
 * <dd>The <i>initiator_modes</i> to use when registering the profile
 * (optional).</dd>
 *
 * <dt>SYSLOG_RAW_LMODES</dt>
 * <dd>The <i>listener_modes</i> to use when registering the profile
 * (optional).</dd>
 *
 * <dt>SYSLOG_RAW_DEBUG</dt>
 * <dd>A non-zero value to enable debugging (optional).</dd>
 * </dl></blockquote>
 *
 * @return A pointer to a profile-registration structure.
 **/
extern PROFILE_REGISTRATION* sr_Init (struct configobj* appconfig);


/* listener routines */

/**
 * Configures a profile-registration structure to behave as a listener.
 * <p>
 * Call this before registering the profile with the wrapper
 * (e.g., before calling {@link tcp_bp_listen tcp_bp_listen}.)
 *
 * @param pr A pointer to the profile-registration returned by
 *           {@link sr_Init sr_Init}.
 *
 * @param callback The routine to invoke on an event
 *                 (cf., {@link (*sr_callback) callback}).
 * 
 * @param clientData A user-supplied pointer.
 **/
extern void sr_listener (PROFILE_REGISTRATION* pr,
                         sr_callback callback,
                         void* clientData);


/* initiator routines */

/**
 * Configures a profile-registration structure to behave as an initiator.
 * <p>
 * Call this before registering the profile with the wrapper
 * (e.g., before calling {@link tcp_bp_connect tcp_bp_connect}.)
 *
 * @param pr A pointer to the profile-registration returned by
 *           {@link sr_Init sr_Init}.
 *
 * @param serverName The value to use for the <i>serverName</i> attribute
 *                    of the &lt;start&gt; element, or <b>NULL</b>.
 *
 * @param callback The routine to invoke on an event
 *                 (cf., {@link (*sr_callback) callback}).
 * 
 * @param clientData A user-supplied pointer.
 **/
extern void sr_initiator (PROFILE_REGISTRATION* pr,
                          char* serverName,
                          sr_callback callback,
                          void* clientData);

/**
 * Sends an entry to log.
 *
 * @param v An opaque pointer returned by the last
 * {@link (*sr_callback) callback}.
 *
 * @param entry The entries to log.
 *
 * @return One of:
 *         <ul>
 *         <li>{@link SR_OK SR_OK}</li>
 *         <li>{@link SR_ERROR SR_ERROR}</li>
 *         <li>{@link SR_BUSY SR_BUSY}</li>
 *         <li>{@link SR_DONE SR_DONE}</li>
 *         </ul>
 **/
extern int sr_log (void* v,
                   char* entry);


/**
 * Closes the SYSLOG reliable RAW channel.
 *
 * @param v An opaque pointer returned by the last
 * {@link (*sr_callback) callback}.
 *
 * @return One of:
 *         <ul>
 *         <li>{@link SR_OK SR_OK}</li>
 *         <li>{@link SR_ERROR SR_ERROR}</li>
 *         <li>{@link SR_BUSY SR_BUSY}</li>
 *         <li>{@link SR_DONE SR_DONE}</li>
 *         </ul>
 **/
extern int sr_fin (void* v);

/**
 * no problema
 **/
#define SR_OK         0

/**
 * error performing task
 **/
#define SR_ERROR      (-1)

/**
 * still doing {@link sr_log sr_log}.
 **/
#define SR_BUSY       (-2)

/**
 * channel is closed
 **/
#define SR_DONE       (-3)

//@}

#if defined(__cplusplus)
}
#endif

#endif
