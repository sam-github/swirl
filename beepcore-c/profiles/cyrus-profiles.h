/* profiles for CYRUS SASL */

/*
 * $Id: cyrus-profiles.h,v 1.6 2002/01/12 23:46:51 mrose Exp $
 */

/*

initiator usage:

   1. cyrus_first ()

   2. sasl_client_init (cyrus_client_callbacks (), pgmname)

   3. pr = cyrus_profiles_Init (appconfig)
      
   4. cyrus_tune (pr, ...)

   5. establish session using pr

   6. d = cyrus_login (...)

      if d is non-NULL, you lose


listener usage:

   1. cyrus_first ()

   2. sasl_server_init (cyrus_server_callbacks (appconfig), pgmname)

   3. pr = cyrus_profiles_Init (appconfig)
      
   4. cyrus_tune (pr, ...)

   5. establish session using pr

 */

#ifndef CYRUS_PROFILES_H
#define CYRUS_PROFILES_H 1

/* includes */

#include "../threaded_os/wrapper/bp_wrapper.h"
#include "../threaded_os/utility/bp_config.h"
#include "tuning.h"
#include "sasl/sasl.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @name Cyrus SASL profiles
 **/

//@{

/* typedefs */

/**
 * Prototype for the connection creation callback
 * 
 * This calls <i>sasl_client_new</i> or <i>sasl_server_new</i> with whatever
 * callback information is desired, and, on success, makes a call to
 * <i>sasl_setprop</i> to set the desired security properties with
 * <b>SASL_SEC_PROPS</b>.
 * <p>
 * Another reason to supply this callback, is it allows you to squirrel away
 * the connection pointer to later call <i>sasl_getprop</i>.
 *
 * @param bp A pointer to the connection structure.
 *
 * @param conn Must be filled-in by the callback, i.e., by calling
 *             <i>sasl_client_new</i> or <i>sasl_server_new</i>.
 *
 * @param clientData The user-supplied pointer provided to
 *                   {@link cyrus_tune cyrus_tune} or
 *                   {@link cyrus_login cyrus_login}.
 *
 * @param serverP If non-zero, call <i>sasl_server_new</i>
 *               (otherwise, call <i>sasl_client_new</i>).
 *
 * @return A SASL result code (e.g., <b>SASL_OK</b>).
 **/
typedef int (*sasl_conn_callback_t) (struct BP_CONNECTION* bp,
                                     sasl_conn_t** conn,
                                     void* clientData,
                                     int serverP);

/**
 * Prototype for the client interaction callback
 * 
 * This is called whenever a client interaction is necessary.
 *
 * @param bp A pointer to the connection structure.
 *
 * @param interact As provided by <i>sasl_client_start</i>  or
 *                 <i>sasl_client_step</i>.
 *
 * @param clientData The user-supplied pointer provided to
 *                   {@link cyrus_login cyrus_login}.
 *
 * @return On failure, a pointer to a {@link diagnostic diagnostic} structure
 *         explaining the reason
 *         (which will subsequently be destroyed by calling
 *         {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 **/
typedef struct diagnostic* (*sasl_interact_callback_t) (BP_CONNECTION* bp,
                                                        sasl_interact_t* interact,
                                                        void* clientData);



/* keys for configuration package */

/**
 * Enables debug mode for the SASL family of profiles,
 * defaults to <b>"0"</b>.
 **/
#define SASL_CYRUS_DEBUG        "beep profiles sasl debug"

/**
 * Specifies the registered name of the service using SASL,
 * defaults to <b>"beep"</b>.
 **/
#define SASL_CYRUS_SERVICE      "beep profiles sasl cyrus service"

/**
 * Specifies the minimum SSF that may be negotiated
 * defaults to <b>0</b>.
 * This is not consulted if {@link cyrus_login cyrus_login} was given
 * a non-NULL <i>callback1</i> parameter.
 **/
#define SASL_CYRUS_MINSSF      "beep profiles sasl cyrus min_ssf"

/**
 * Specifies the maximum SSF that may be negotiated
 * defaults to <b>256</b>.
 * This is not consulted if {@link cyrus_login cyrus_login} was given
 * a non-NULL <i>callback1</i> parameter.
 **/
#define SASL_CYRUS_MAXSSF      "beep profiles sasl cyrus max_ssf"

/**
 * Specifies the registered name of the service using SASL,
 * optional.
 **/
#define SASL_CYRUS_PLUGINPATH   "beep profiles sasl cyrus plugin_path"


/* module entry points */

/**
 * Well-known entry point for the SASL profiles implemented by Cyrus.
 * <p>
 * Note that you <b>must</b> call <i>sasl_server_init</i> or
 * <i>sasl_client_init</i> <b>before</b> you call this routine.
 * (But, first of all, you <b>must</b> call {@link cyrus_first cyrus_first}.)
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure used for configuration purposes.
 * <p>
 * Keys:
 * <blockquote><dl>
 * <dt>SASL_CYRUS_DEBUG</dt>
 * <dd>A non-zero value to enable debugging (optional).</dd>
 *
 * <dt>SASL_CYRUS_SERVICE</dt>
 * <dd>The registered name of the service using SASL,
 * defaults to <b>"beep"</b>.</dd>
 * </dl></blockquote>
 *
 * @return A pointer to a linked-list of profile-registration structures.
 **/
extern struct PROFILE_REGISTRATION* cyrus_profiles_Init(struct configobj* appconfig);


/* additional profile-related functions */

/**
 * The function to call before calling <i>sasl_client_init</i> or
 * <i>sasl_server_init</i>.
 **/
extern void cyrus_first (void);

/**
 * Returns a <i>sasl_callback_t</i> array, suitable for passing to
 * <i>sasl_client_init</i>.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure used for configuration purposes.
 * <p>
 * Keys:
 * <blockquote><dl>
 * <dt>SASL_CYRUS_PLUGINPATH</dt>
 * <dd>A colon-separated search path (optional).</dd>
 * </dl></blockquote>
 *
 * @return A pointer to a static array.
 **/
extern sasl_callback_t* cyrus_client_callbacks (struct configobj *appconfig);

/**
 * Returns a <i>sasl_callback_t</i> array, suitable for passing to
 * <i>sasl_server_init</i>.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure used for configuration purposes.
 * <p>
 * Keys:
 * <blockquote><dl>
 * <dt>SASL_CYRUS_PLUGINPATH</dt>
 * <dd>A colon-separated search path (optional).</dd>
 * </dl></blockquote>
 *
 * @return A pointer to a static array.
 **/
extern sasl_callback_t* cyrus_server_callbacks (struct configobj *appconfig);

/**
 * Sets the <i>SASL_CB_LOG</i> callback to use the logging package.
 *
 * @param bp A pointer to the connection structure.
 *
 * @param cb The callback array to update.
 **/
extern int cyrus_callback_setlog (BP_CONNECTION* bp,
                                  sasl_callback_t* cb);

/**
 * A default interaction-handler.
 * <p>
 * Consult the connection's {@link configobj configuration}
 * structure for the relevant information.
 * If absent, then it queries the user via <i>stdin</i>.
 * <p>
 * Input Keys, all optional:
 * <blockquote><dl>
 * <dt>SASL_LOCAL_REALM</dt>
 * <dd>The authentication realm.</dd>
 *
 * <dt>SASL_LOCAL_USERNAME</dt>
 * <dd>The SASL authentication-identity.</dd>
 *
 * <dt>SASL_LOCAL_TARGET</dt>
 * <dd>The SASL authorization-identity.</dd>
 *
 * <dt>SASL_LOCAL_PASSPHRASE</dt>
 * <dd>The passphrase for the authentication-identity.</dd>
 * </dl></blockquote>
 * <p>
 * Consult the {@link (*sasl_interact_callback_t) callback} information
 * for an explanation of the parameters and return value.
 *
 * @see cyrus_login
 **/
extern struct diagnostic* cyrus_fillin (BP_CONNECTION* bp,
                                        sasl_interact_t* interact,
                                        void* clientData);


/**
 * Configures a profile-registration structure to invoke a user-supplied
 * callback when it wants to create a <i>sasl_conn_t</i>.
 * <p>
 * Call this before registering the profile with the wrapper
 * (e.g., before calling {@link tcp_bp_listen tcp_bp_listen}.)
 *
 * @param pr A pointer to the profile-registration returned by
 *           {@link cyrus_profiles_Init cyrus_profiles_Init}.
 *
 * @param callback The routine to invoke to call <i>sasl_client_new</i>
 *                 or <i>sasl_server_new</i>
 *                 (cf., {@link (*sasl_conn_callback_t) callback}).
 *
 * @param clientData A user-supplied pointer, supplied to <i>callback</i>.
 * 
 * @param allP If non-zero, traverse the entire chain of profile-registration
 *              structures.
 **/
extern void cyrus_tune (PROFILE_REGISTRATION* pr,
                        sasl_conn_callback_t callback,
                        void* clientData,
                        int allP);

/**
 * Tune using SASL, and update the connection's
 * {@link configobj configuration} structure accordingly.
 *
 * @param bp A pointer to the connection structure.
 * <p>
 * Input Keys:
 * <blockquote><dl>
 * <dt>SASL_CYRUS_SERVICE</dt>
 * <dd>The "service name" for this application,
 * defaults to <b>"beep"</b>.</dd>
 *
 * <dt>SASL_LOCAL_MECHANISM</dt>
 * <dd>The SASL mechanism to use; if not present, this is negotiated.</dd>
 * </dl></blockquote>
 * <p>
 * Output Keys:
 * <blockquote><dl>
 * <dt>SASL_LOCAL_MECHANISM</dt>
 * <dd>The SASL mechanism used for authentication.</dd>
 *
 * <dt>SASL_LOCAL_CODE</dt>
 * <dd>The resulting status code.
 * Consult \URL[Section 8 of RFC 3080]{http://www.beepcore.org/beepcore/docs/rfc3080.jsp#reply-codes}
 * for an (incomplete) list.</dd>
 *
 * <dt>SASL_LOCAL_REASON</dt>
 * <dd>A textual message corresponding to <b>SASL_LOCAL_CODE</b></dd>
 * </dl></blockquote>
 *
 * @param serverName The value to use for the <i>serverName</i> attribute
 *                    of the &lt;start&gt; element (may not be <b>NULL</b>).
 *
 * @param callback1 The routine to invoke to call <i>sasl_client_new</i>
 *                 or <i>sasl_server_new</i>
 *                 (cf., {@link (*sasl_conn_callback_t) callback}).
 *
 * @param clientData1 A user-supplied pointer, supplied to <i>callback1</i>.
 * 
 * @param callback2 The routine to invoke when a client interaction is needed
 *                 (cf., {@link (*sasl_interact_callback_t) callback}),
 *                 if <b>NULL</b>,
 *                 then {@link cyrus_fillin cyrus_fillin} is used.
 *
 * @param clientData2 A user-supplied pointer, supplied to the <i>callback</i>.
 * 
 * @return On failure, a pointer to a {@link diagnostic diagnostic} structure
 *         explaining the reason
 *         (which should subsequently be destroyed by calling
 *         {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 *
 * @see cyrus_fillin
 **/
extern struct diagnostic* cyrus_login(BP_CONNECTION* bp,
                                      char* serverName,
                                      sasl_conn_callback_t callback1,
                                      void* clientData1,
                                      sasl_interact_callback_t callback2,
                                      void* clientData2);

//@}

#if defined(__cplusplus)
}
#endif
#endif
