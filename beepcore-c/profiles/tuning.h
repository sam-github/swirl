/* tuning parameters */

#ifndef TUNING_H
#define TUNING_H        1

/* includes */

#include "../threaded_os/utility/bp_config.h"


#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * @name Tuning Parameters
 **/

//@{

/**
 * The prefix used for identity properties associated with the local peer.
 **/
#define SASL_LOCAL_PREFIX      "beep identity local "

/**
 * The SASL authentication-identity.
 **/
#define SASL_LOCAL_USERNAME     "beep identity local username"

/**
 * The SASL authorization-identity.
 **/
#define SASL_LOCAL_TARGET       "beep identity local target"

/**
 * The authentication realm.
 **/
#define SASL_LOCAL_REALM        "beep identity local target"

/**
 * The passphrase for the authentication-identity.
 **/
#define SASL_LOCAL_PASSPHRASE   "beep identity local passphrase"

/**
 * The mechanism used during the peer's attempt to authenticate.
 * <p>
 * Typically, this is set by {@link sasl_login sasl_login}
 * or {@link cyrus_login cyrus_login}.
 **/
#define SASL_LOCAL_MECHANISM   "beep identity local mechanism"

/**
 * The reply code resulting from a peer's attempt to authenticate.
 * <p>
 * Typically, this is set by {@link sasl_login sasl_login}
 * or {@link cyrus_login cyrus_login}.
 * Consult \URL[Section 8 of RFC 3080]{http://www.beepcore.org/beepcore/docs/rfc3080.jsp#reply-codes}
 * for an (incomplete) list.
 **/
#define SASL_LOCAL_CODE         "beep identity local reply_code"

/**
 * A textual message corresponding to the
 * {@link SASL_LOCAL_CODE SASL_LOCAL_CODE}.
 * <p>
 * Typically, this is set by {@link sasl_login sasl_login}
 * or {@link cyrus_login cyrus_login}.
 **/
#define SASL_LOCAL_REASON       "beep identity local reply_text"

/**
 * The prefix used for identity properties associated with the remote peer.
 **/
#define SASL_REMOTE_PREFIX      "beep identity remote "

/**
 * The mechanism used during the peer's attempt to authenticate.
 **/
#define SASL_REMOTE_MECHANISM   "beep identity remote mechanism"

/**
 * The SASL authentication-identity of the peer.
 * <p>
 * After registering the result from
 * {@link sasl_profiles_Init sasl_profiles_Init}
 * or {@link cyrus_profiles_Init cyrus_profiles_Init},
 * this parameter gets set when appropriate.
 **/
#define SASL_REMOTE_USERNAME    "beep identity remote username"

/**
 * The SASL authorization-identity of the peer.
 * <p>
 * After registering the result from
 * {@link sasl_profiles_Init sasl_profiles_Init}
 * or {@link cyrus_profiles_Init cyrus_profiles_Init},
 * this parameter gets set when appropriate.
 **/
#define SASL_REMOTE_TARGET      "beep identity remote target"

/**
 * The numeric result of the peer's attempt to authenticate.
 * <p>
 * After registering the result from
 * {@link sasl_profiles_Init sasl_profiles_Init}
 * or {@link cyrus_profiles_Init cyrus_profiles_Init},
 * this parameter gets set when appropriate.
 * Consult \URL[Section 8 of RFC 3080]{http://www.beepcore.org/beepcore/docs/rfc3080.jsp#reply-codes}
 * for an (incomplete) list.
 **/
#define SASL_REMOTE_CODE        "beep identity remote reply_code"

/**
 * A textual message corresponding to the
 * {@link SASL_REMOTE_CODE SASL_REMOTE_CODE}.
 * <p>
 * After registering the result from
 * {@link sasl_profiles_Init sasl_profiles_Init}
 * or {@link cyrus_profiles_Init cyrus_profiles_Init},
 * this parameter gets set when appropriate.
 **/
#define SASL_REMOTE_REASON      "beep identity remote reply_text"


/**
 * The prefix used for privacy properties associated with the session.
 **/
#define PRIVACY_PREFIX          "beep identity privacy "

/**
 * The name of the cipher in use.
 * <p>
 * On the client-side, this is typically set as a result of calling
 * {@link tls_privatize tls_privatize}; whilst on the server-side,
 * after registering the result from {@link tls_profile_Init tls_profile_Init},
 * this parameter gets set when appropriate.
 * If the Cyrus SASL package is in use, then this may also be set as a result
 * of calling {@link cyrus_login cyrus_login}; whilst on the server-side,
 * after registering the result from
 * {@link cyrus_profiles_Init cyrus_profiles_Init}.
 **/
#define PRIVACY_CIPHER_NAME     "beep identity privacy cipher name"

/**
 * The number of bits in the session key in use.
 * <p>
 * On the client-side, this is typically set as a result of calling
 * {@link tls_privatize tls_privatize}; whilst on the server-side,
 * after registering the result from {@link tls_profile_Init tls_profile_Init},
 * this parameter gets set when appropriate.
 * If the Cyrus SASL package is in use, then this may also be set as a result
 * of calling {@link cyrus_login cyrus_login}; whilst on the server-side,
 * after registering the result from
 * {@link cyrus_profiles_Init cyrus_profiles_Init}.
 **/
#define PRIVACY_CIPHER_SBITS    "beep identity privacy cipher session_bits"

/**
 * The name of the privacy protocol in use.
 * <p>
 * On the client-side, this is typically set as a result of calling
 * {@link tls_privatize tls_privatize}; whilst on the server-side,
 * after registering the result from {@link tls_profile_Init tls_profile_Init},
 * this parameter gets set when appropriate.
 **/
#define PRIVACY_CIPHER_PROTOCOL "beep identity privacy cipher protocol"

/**
 * The subject name from the peer's certificate.
 * <p>
 * On the client-side, this is typically set as a result of calling
 * {@link tls_privatize tls_privatize}; whilst on the server-side,
 * after registering the result from {@link tls_profile_Init tls_profile_Init},
 * this parameter gets set when appropriate.
 **/
#define PRIVACY_REMOTE_SUBJECTNAME \
                                "beep identity privacy remote subject_name"

//@}

#if defined(__cplusplus)
}
#endif

#endif
