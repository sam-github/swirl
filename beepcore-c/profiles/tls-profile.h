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
 * $Id: tls-profile.h,v 1.19 2002/01/20 23:03:40 mrose Exp $
 */

/*

initiator usage:

   1. pr = tls_profile_Init (appconfig)

   2. establish session using pr

   3. d = tls_privatize (...)

      if d is non-NULL, you lose


listener usage:

   1. pr = tls_profile_Init (appconfig)

   2. establish session using pr

 */

#ifndef TLS_PROFILE_H
#define TLS_PROFILE_H 1

/* includes */

#include "../threaded_os/wrapper/bp_wrapper.h"
#include "../threaded_os/utility/bp_config.h"
#include "tuning.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @name The TLS profile
 **/

//@{

/* keys for configuration package */

/**
 * URI to use for the TLS profile,
 * defaults to <b>"http://iana.org/beep/TLS"</b>.
 **/
#define TLS_URI                 "beep profiles tls uri"

/**
 * File containing the public key certificate,
 * must be present.
 **/
#define TLS_CERTFILE            "beep profiles tls certfile"

/**
 * Directory containing public key certificates,
 * each file named according to the <i>serverName</i> attribute.
 * If this key or the resulting file isn't present,
 * <i>TLS_CERTFILE</i> is used instead.
 **/
#define TLS_CERTDIR            "beep profiles tls certdir"

/**
 * File containing the secret key,
 * must be present.
 **/
#define TLS_KEYFILE             "beep profiles tls keyfile"

/**
 * Directory containing secret keys,
 * each file named according to the <i>serverName</i> attribute.
 * If this key or the resulting file isn't present,
 * <i>TLS_KEYFILE</i> is used instead.
 **/
#define TLS_KEYDIR              "beep profiles tls keydir"

/**
 * Enables debug mode for the TLS profile,
 * defaults to <b>"0"</b>.
 **/
#define TLS_DEBUG               "beep profiles tls debug"



/* module entry points */

/**
 * Well-known entry point for the TLS profile.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure used for configuration purposes.
 * <p>
 * Keys:
 * <blockquote><dl>
 * <dt>TLS_URI</dt>
 * <dd>The registration URI for the profile.</dd>
 *
 * <dt>TLS_DEBUG</dt>
 * <dd>A non-zero value to enable debugging (optional).</dd>
 *
 * <dt>TLS_CERTDIR</dt>
 * <dd>The name of the directory containing public key certificates,
 * each file named according to the <i>serverName</i> attribute
 * (optional, server-side only).
 * If this key or the resulting file isn't present,
 * <i>TLS_CERTFILE</i> is used instead.</dd>
 *
 * <dt>TLS_KEYFILE</dt>
 * <dd>The name of the file containing the private key.</dd>
 *
 * <dt>TLS_KEYDIR</dt>
 * <dd>The name of the directory containing secret keys,
 * each file named according to the <i>serverName</i> attribute
 * (optional, server-side only).
 * If this key or the resulting file isn't present,
 * <i>TLS_KEYFILE</i> is used instead.</dd>
 * </dl></blockquote>
 *
 * @return A pointer to a profile-registration structure.
 **/
extern struct PROFILE_REGISTRATION* tls_profile_Init(struct configobj* appconfig);

/**
 * Tune using TLS, and update the connection's
 * {@link configobj configuration} structure accordingly.
 * <p>
 * On success, transport privacy is in effect and no user channels are open.
 *
 * @param bp A pointer to the connection structure. 
 * <p>
 * Input Keys:
 * <blockquote><dl>
 * <dt>TLS_CERTFILE</dt>
 * <dd>The name of the file containing the public key certificate.</dd>
 *
 * <dt>TLS_KEYFILE</dt>
 * <dd>The name of the file containing the private key.</dd>
 * </dl></blockquote>
 *
 * <p>
 * Output Keys:
 * <blockquote><dl>
 * <dt>PRIVACY_REMOTE_SUBJECTNAME</dt>
 * <dd>The subject name from the peer's certificate.</dd>
 *
 * <dt>PRIVACY_CIPHER_NAME</dt>
 * <dd>The name of the cipher in use.</dd>
 *
 * <dt>PRIVACY_CIPHER_PROTOCOL</dt>
 * <dd>The name of the privacy protocol in use.</dd>
 *
 * <dt>PRIVACY_CIPHER_SBITS</dt>
 * <dd>The number of bits in the secret key in use.</dd>
 * </dl></blockquote>
 *
 * @param serverName The value to use for the <i>serverName</i> attribute
 *                    of the &lt;start&gt; element, or <b>NULL</b>.
 *
 * @return On failure, a pointer to a {@link diagnostic diagnostic} structure
 *         explaining the reason
 *         (which should subsequently be destroyed by calling
 *         {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 **/
extern struct diagnostic* tls_privatize(struct BP_CONNECTION* bp,
                                        char* serverName);

//@}

#if defined(__cplusplus)
}
#endif

#endif
