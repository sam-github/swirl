/* profiles for NULL ECHO/SINK */

/*
 * $Id: null-profiles.h,v 1.12 2002/01/02 02:11:10 mrose Exp $
 */

/*

initiator usage:

   1. pr = null_echo_Init (appconfig)    or null_sink_Init ...
      
   2. establish session, bp, using pr

   3. v = null_start (bp, pr)

      if v is NULL, you lose

   4. for 0..I: cc = null_trip (v, ibuf, ilen, obuf, omaxlen)

       if (cc < 0) release session, etc.

       otherwise cc octets were stored in obuf

   5. i = null_close (v)

       if (i < 0) release session, etc.

       otherwise channel is closed


listener usage:

   1. pr = null_echo_Init (appconfig)    or null_sink_Init ...
      
   2. establish session using pr

   3. sr_Fin (pr) prior to destroying pr

 */

#ifndef NULL_PROFILES_H
#define NULL_PROFILES_H 1


/* includes */

#include "../threaded_os/utility/bp_config.h"
#include "../threaded_os/wrapper/bp_wrapper.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * @name The NULL/ECHO and NULL/SINK profiles
 **/

//@{

/* keys for configuration package */

/**
 * URI to use for the NULL ECHO profile,
 * defaults to <b>"http://xml.resource.org/profiles/NULL/ECHO"</b>.
 **/
#define NULL_ECHO_URI           "beep profiles null_echo uri"

/**
 * Initiator modes for the NULL ECHO profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define NULL_ECHO_IMODES        "beep profiles null_echo initiator_modes"

/**
 * Listener modes for the NULL ECHO profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define NULL_ECHO_LMODES        "beep profiles null_echo listener_modes"

/**
 * Enables debug mode for the NULL ECHO profile,
 * defaults to <b>"0"</b>.
 **/
#define NULL_ECHO_DEBUG         "beep profiles null_echo debug"

/**
 * URI to use for the NULL SINK profile,
 * defaults to <b>"http://xml.resource.org/profiles/NULL/SINK"</b>.
 **/
#define NULL_SINK_URI           "beep profiles null_sink uri"

/**
 * Initiator modes for the NULL SINK profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define NULL_SINK_IMODES        "beep profiles null_sink initiator_modes"

/**
 * Listener modes for the NULL SINK profile,
 * defaults to <b>"plaintext,encrypted"</b>.
 **/
#define NULL_SINK_LMODES        "beep profiles null_sink listener_modes"

/**
 * Enables debug mode for the NULL SINK profile,
 * defaults to <b>"0"</b>.
 **/
#define NULL_SINK_DEBUG         "beep profiles null_sink debug"



/* module entry points */

/**
 * Well-known entry point for the NULL/ECHO profile.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure used for configuration purposes.
 * <p>
 * Keys:
 * <blockquote><dl>
 * <dt>NULL_ECHO_URI</dt>
 * <dd>The registration URI for the profile (optional).</dd>
 *
 * <dt>NULL_ECHO_IMODES</dt>
 * <dd>The <i>initiator_modes</i> to use when registering the profile
 * (optional).</dd>
 *
 * <dt>NULL_ECHO_LMODES</dt>
 * <dd>The <i>listener_modes</i> to use when registering the profile
 * (optional).</dd>
 * </dl></blockquote>
 *
 * @return A pointer to a profile-registration structure.
 **/
extern struct PROFILE_REGISTRATION* null_echo_Init (struct configobj* appconfig);

/**
 * Well-known entry point for the NULL/SINK profile.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure that may be used for configuration purposes.
 * <p>
 * Keys:
 * <blockquote><dl>
 * <dt>NULL_SINK_URI</dt>
 * <dd>The registration URI for the profile (optional).</dd>
 *
 * <dt>NULL_SINK_IMODES</dt>
 * <dd>The <i>initiator_modes</i> to use when registering the profile
 * (optional).</dd>
 *
 * <dt>NULL_SINK_LMODES</dt>
 * <dd>The <i>listener_modes</i> to use when registering the profile
 * (optional).</dd>
 * </dl></blockquote>
 *
 * @return A pointer to a profile-registration structure.
 **/
extern struct PROFILE_REGISTRATION* null_sink_Init (struct configobj* appconfig);


/* initiator routines */

/**
 * Starts an echo or sink channel.
 *
 * @param bp A pointer to the connection structure.
 *
 * @param pr A pointer to the profile-registration returned by either
 *          {@link null_echo_Init null_echo_Init} or
 *           {@link null_sink_Init null_sink_Init}.
 *
 * @param serverName The value to use for the <i>serverName</i> attribute
 *                    of the &lt;start&gt; element, or <b>NULL</b>.
 *
 * @return An opaque pointer for use with {@link null_trip null_trip} and
 *         {@link null_close null_close}.
 **/
extern void* null_start (struct BP_CONNECTION* bp,
                         struct PROFILE_REGISTRATION* pr,
                         char* serverName);

/**
 * Initiates a round-trip transaction.
 *
 * @param v An opaque pointer returned by {@link null_start null_start}.
 *
 * @param ibuf A character pointer to the send buffer.
 *
 * @param ilen The length of the send buffer, in octets.
 *
 * @param obuf A character pointer to the receive buffer.
 *
 * @param omaxlen The length of the receive buffer, in octets.
 *
 * @return The number of octets received, or one of:
 *         <ul>
 *         <li>{@link NULL_ERROR NULL_ERROR}</li>
 *         <li>{@link NULL_BUSY NULL_BUSY}</li>
 *         <li>{@link NULL_DONE NULL_DONE}</li>
 *         </ul>
 **/
extern int null_trip (void* v,
                      char* ibuf,
                      int ilen,
                      char* obuf,
                      int omaxlen);

/**
 * Closes an echo or sink channel.
 *
 * @param v An opaque pointer returned by {@link null_start null_start}.
 *
 * @return One of:
 *         <ul>
 *         <li>{@link NULL_OK NULL_OK}</li>
 *         <li>{@link NULL_ERROR NULL_ERROR}</li>
 *         <li>{@link NULL_BUSY NULL_BUSY}</li>
 *         <li>{@link NULL_DONE NULL_DONE}</li>
 *         <li>{@link NULL_DENIED NULL_DENIED}</li>
 *         </ul>
 **/
extern int null_close (void* v);

/**
 * no problema
 **/
#define NULL_OK         0

/**
 * error performing task
 **/
#define NULL_ERROR      (-1)

/**
 * still doing {@link null_trip null_trip}
 **/
#define NULL_BUSY       (-2)

/**
 * channel is closed
 **/
#define NULL_DONE       (-3)

/**
 * remote peer refused to close
 **/
#define NULL_DENIED     (-4)

//@}

#if defined(__cplusplus)
}
#endif

#endif
