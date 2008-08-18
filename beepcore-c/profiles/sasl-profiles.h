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
 * $Id: sasl-profiles.h,v 1.4 2002/01/02 02:11:10 mrose Exp $
 */

/*

initiator usage:

   1. pr = sasl_profiles_Init (appconfig)

   2. establish session using pr

   3. d = sasl_login (...)

      if d is non-NULL, you lose


listener usage:

   1. pr = sasl_profiles_Init (appconfig)

   2. establish session using pr

 */

#ifndef SASL_PROFILES_H
#define SASL_PROFILES_H 1

/* includes */

#include "../threaded_os/wrapper/bp_wrapper.h"
#include "../threaded_os/utility/bp_config.h"
#include "tuning.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @name The SASL ANONYMOUS and OTP profiles
 **/

//@{

/* keys for configuration package */

/**
 * URI to use for the SASL ANONYMOUS profile,
 * defaults to <b>"http://iana.org/beep/SASL/ANONYMOUS"</b>.
 **/
#define SASL_ANONYMOUS_URI      "beep profiles sasl_anon uri"

/**
 * Initiator modes for the SASL ANONYMOUS profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define SASL_ANONYMOUS_IMODES   "beep profiles sasl_anon initiator_modes"

/**
 * Listener modes for the SASL ANONYMOUS profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define SASL_ANONYMOUS_LMODES   "beep profiles sasl_anon listener_modes"

/**
 * URI to use for the SASL OTP profile,
 * defaults to <b>"http://iana.org/beep/SASL/OTP"</b>.
 **/
#define SASL_OTP_URI            "beep profiles sasl_otp uri"

/**
 * Initiator modes for the SASL OTP profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define SASL_OTP_IMODES         "beep profiles sasl_otp initiator_modes"

/**
 * Listener modes for the SASL OTP profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define SASL_OTP_LMODES         "beep profiles sasl_otp listener_modes"

/**
 * The directory where the OTP database is kept,
 * defaults to <b>"/tmp"</b>.
 **/
#define SASL_OTP_DIRECTORY      "beep profiles sasl_otp directory"

/**
 * Enables debug mode for the SASL family of profiles,
 * defaults to <b>"0"</b>.
 **/
#define SASL_FAMILY_DEBUG       "beep profiles sasl debug"


/**
 * Trace information for SASL ANONYMOUS.
 **/
#define SASL_LOCAL_TRACEINFO    "beep identity local traceinfo"


/* module entry points */

/**
 * Well-known entry point for the SASL ANONYMOUS and OTP profiles.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure used for configuration purposes.
 * <p>
 * Keys:
 * <blockquote><dl>
 * <dt>SASL_ANONYMOUS_URI/SASL_OTP_URI</dt>
 * <dd>The registration URI for the profile (optional).</dd>
 *
 * <dt>SASL_ANONYMOUS_IMODES/SASL_OTP_IMODES</dt>
 * <dd>The <i>initiator_modes</i> to use when registering the profile
 * (optional).</dd>
 *
 * <dt>SASL_ANONYMOUS_LMODES/SASL_OTP_LMODES</dt>
 * <dd>The <i>listener_modes</i> to use when registering the profile
 * (optional).</dd>
 *
 * <dt>SASL_OTP_DIRECTORY</dt>
 * <dd>The directory where OTP information is kept,
 * defaults to <b>"/tmp"</b>.</dd>
 *
 * <dt>SASL_FAMILY_DEBUG</dt>
 * <dd>A non-zero value to enable debugging (optional).</dd>
 * </dl></blockquote>
 *
 * @return A pointer to a linked-list of profile-registration structures.
 **/
extern struct PROFILE_REGISTRATION* sasl_profiles_Init(struct configobj* appconfig);

/**
 * Tune using SASL, and update the connection's
 * {@link configobj configuration} structure accordingly.
 *
 * @param bp A pointer to the connection structure.
 * <p>
 * Input Keys:
 * <blockquote><dl>
 * <dt>SASL_LOCAL_MECHANISM</dt>
 * <dd>One of: <b>"anonymous"</b>, <b>"otp"</b>.</dd>
 *
 * <dt>SASL_LOCAL_TRACEINFO</t>
 * <dd>Trace information for SASL ANONYMOUS.</dd>
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
 *                    of the &lt;start&gt; element, or <b>NULL</b>.
 *
 * @return On failure, a pointer to a {@link diagnostic diagnostic} structure
 *         explaining the reason
 *         (which should subsequently be destroyed by calling
 *         {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 **/
extern struct diagnostic* sasl_login(BP_CONNECTION* bp,
                                     char* serverName);

//@}

#if defined(__cplusplus)
}
#endif
#endif
