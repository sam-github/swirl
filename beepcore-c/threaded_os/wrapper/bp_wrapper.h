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
 * $Id: bp_wrapper.h,v 1.44 2002/04/27 04:40:22 huston Exp $
 *
 * IW_wrapper.h
 *
 * Functionality for the wrapper library for the BEEP core library.
 * Provided here are the interfaces to the application for managing a
 * BEEP connection.  The wrapper provides an abstration layer for
 * memory managememt to the underlying routines and may also 
 * implement policies on profile stratup enhancing the default 
 * behaviour as described in the RFC.
 */
#ifndef BP_WRAPPER_H
#define BP_WRAPPER_H 1

#include "../utility/semutex.h"
#include "../../core/generic/CBEEP.h"
#include "../../utility/bp_slist_utility.h"
#include "../../utility/bp_malloc.h"
#include "../utility/bp_config.h"
#include "../transport/bp_fiostate.h"
#include "../transport/bp_fpollmgr.h" 

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * typedefs of convenience
 */
typedef struct frame FRAME;
typedef struct diagnostic DIAGNOSTIC;

/*
 * Wrapper public struct definitions.
 */
typedef struct profile PROFILE;

/*
 * Public structures.
 */

/**
 * @name Core data structures
 **/

//@{
//@Include: ../../core/generic/CBEEP.h
//@}

/**
 * @name Connection structure and functions
 **/

//@{

/**
 * A structure defined by the Wrapper API for managing a connection.
 * <p>
 * The code that manages the
 * BP_CONNECTION structure is called the wrapper. Its
 * responsibilities include mapping of channel numbers to profile-instances,
 * mapping of URIs to profile registrations, and invoking
 * profiles at the appropriate time as messages arrive on the
 * socket. It also manages input/output threads, profile threads, and
 * locks to prevent conflicts. It manages tuning resets and graceful
 * termination of sessions and controls.  Finally, the wrapper
 * indirectly manages any information that is inter-channel in nature,
 * such as the SASL identity, the TLS encryption keys, and any other
 * information a profile might wish to make available to other
 * profiles.
 *
 * @field role Either <b>INST_ROLE_INITIATOR</b> or <b>INST_ROLE_LISTENER</b>
 *
 * @field mode Either <b>"plaintext"</b> or <b>"encrypted"</b>
 *
 * @field status The connection's current status, one of:
 *		 <ul>
 *		 <li>INST_STATUS_STARTING</li>
 *		 <li>INST_STATUS_CLOSING</li>
 *		 <li>INST_STATUS_TUNING</li>
 *		 <li>INST_STATUS_EXIT</li>
 *		 <li>INST_STATUS_RUNNING</li>
 *		 </ul>
 *
 * @field channel_instance A pointer to the channels known (started or being
 *			   started) on this connection. 
 **/
struct BP_CONNECTION 
{
    char  role;
    char *mode;
    char status;

/**
 * BP_CONNECTION was initiated by the local peer.
 **/
#define INST_ROLE_INITIATOR               'I'

/**
 * BP_CONNECTION was initiated by the remote peer.
 **/
#define INST_ROLE_LISTENER                'L'

/**
 * BP_CONNECTION is in the process of starting a new channel, possibly
 * channel 0.
 **/
#define INST_STATUS_STARTING              'S'

/**
 * BP_CONNECTION is in the process of closing an existing channel.
 **/
#define INST_STATUS_CLOSING               'C'

/**
 * BP_CONNECTION is starting a tuning reset.
 **/
#define INST_STATUS_TUNING                'T'

/**
 * BP_CONNECTION is trying to shut down.
 **/
#define INST_STATUS_EXIT                  'X'

/**
 * BP_CONNECTION isn't in any other state.
 **/
#define INST_STATUS_RUNNING               'R'

#if 0
    char *(*tuning_reset_callback)(struct BP_CONNECTION *, long);	
#endif
    bp_slist *channel_instance;
};

typedef struct BP_CONNECTION BP_CONNECTION;

//@Include: ../transport/bp_tcp.h

//@}

/**
 * A structure defined by the Profile API for managing a profile-instance.
 * <p>
 * This structure is created
 * by the wrapper each time a channel is opened, and is maintained by
 * the wrapper code.
 * (The wrapper also maintains a dual,
 * the {@link CHANNEL_INSTANCE channel-instance} structure.)
 * <p>
 * This structure contains function pointers for the "inner" callbacks
 * invoked by the wrapper.
 *
 * @field channel A pointer to the associated
 *		 {@link CHANNEL_INSTANCE channel-instance} structure.
 *
 * @field full_messages Indicates whether the profile-instance is using
 *                      the frame-oriented or the message-oriented
 *                      interface.  In the latter case, if the receive
 *                      buffer fills up before a complete message is 
 *                      received, then
 *                      {@link pro_window_full pro_window_full}
 *                      being invoked.
 * 
 * @field user_ptr1 A user-defined pointer.
 *
 * @field user_ptr2 A user-defined pointer.
 *
 * @field user_long1 A user-defined long-value.
 *
 * @field user_long2 A user-defined long-value.
 **/
struct PROFILE_INSTANCE 
{
    struct CHANNEL_INSTANCE *channel;
    int full_messages;
/**
 * @name Profile API functions
 *
 * Defines the function pointers for "inner" callbacks invoked by the wrapper. 
 **/

//@{

    /**
     * @name pro_start_indication
     *
     * Indicates that a &lt;start&gt; element has been received
     * for the specified profile-instance.
     * <p>
     * The profile should handle any
     * start data and call
     * {@link bpc_start_response bpc_start_response}
     * before returning.
     * <p>
     * To determine the <i>servername</i> to use, the callback should first
     * invoke {@link bp_server_name bp_server_name}. If <b>NULL</b>, then
     * consult <i>pi->channel->inbound->server_name</i> instead.
     * 
     * @param pi A pointer to the newly-allocated profile-instance
     * structure created by the wrapper.
     *
     * @param po A pointer to the {@link profile profile}
     * structure, which is filled in
     * with a parsed version of the &lt;start&gt; message received over 
     * channel 0. The contents of this structure should be considered 
     * read-only.
     **/ 
    void (*pro_start_indication)(struct PROFILE_INSTANCE *pi, 
                                 struct profile *po);

    /**
     * @name pro_start_confirmation
     *
     * Confirms that the given channel has been started successfully.
     * <p>
     * This is 
     * invoked after the start request callback has returned. 
     * If the &lt;start&gt; is
     * refused by the remote peer via the return of an &lt;error&gt; element, 
     * then this callback is not made,
     * and only the start request
     * callback is called. On a channel started by the local peer, this is the
     * first function invoked on the new profile-instance.
     *
     * @param client_data A user-supplied pointer from
     *	{@link bp_start_request bp_start_request}
     *
     * @param pi A pointer to the newly-allocated profile-instance
     * structure created by the wrapper.
     *
     * @param po A pointer to the {@link profile profile}
     * structure, which is filled in
     * with a parsed version of the &lt;profile&gt; message received over 
     * channel 0. The contents of this structure should be considered 
     * read-only.
     **/ 
    void (*pro_start_confirmation)(void *client_data,
				   struct PROFILE_INSTANCE *pi, 
                                   struct profile *po);

    /**
     * @name pro_close_indication
     *
     * Indicates that a &lt;close&gt;element has been received
     * for the specified profile-instance, or that the local peer is asking
     * to close this channel or the entire session.
     * <p>
     * The profile should
     * decide if it agrees (or not) and call
     * {@link bpc_close_response bpc_close_response}
     * before returning.
     *
     * @param pi A pointer to the profile-instance structure.
     *
     * @param status A pointer to the {@link diagnostic diagnostic}
     *               structure containing the reason for the request.
     *
     * @param local_remote Indicates which peer is making the request, one of:
     *		 <ul>
     *		 <li>PRO_ACTION_LOCAL</li>
     *		 <li>PRO_ACTION_REMOTE</li>
     *		 </ul>
     *
     * @param session_channel Indicates what is being requested to close, one
     *                        of:
     *		 <ul>
     *		 <li>PRO_ACTION_SESSION</li>
     *		 <li>PRO_ACTION_CHANNEL</li>
     *		 <li>PRO_ACTION_ABORT</li>
     *		 </ul>
     *           Note that in the last case, this is a demand, not a request.
     **/
    void (*pro_close_indication)(struct PROFILE_INSTANCE *pi,
				 struct diagnostic *status,
                                 char local_or_remote,
                                 char session_or_channel);

    /**
     * @name pro_close_confirmation
     *
     * Confirms the final disposition of the close request.
     * <p>
     * This is invoked regardless of who asked to close the channel.
     * (If invoked by the local peer, it's invoked 
     * after the close request callback has returned.)
     * If the channel is now closed, the profile-instance structure is about
     * to be deallocated, so the profile should release any instance-specific
     * resources, e.g., referenced by <b>user_ptr1</b> and <b>user_ptr2</b>.
     *
     * @param pi A pointer to the profile-instance structure.
     *
     * @param commit_or_rollback Whether the close actually occurred, one of:
     *		 <ul>
     *		 <li>PRO_ACTION_SUCCESS</li>
     *		 <li>PRO_ACTION_FAILURE</li>
     *		 </ul>
     *
     * @param error On rollback, a pointer to a  {@link diagnostic diagnostic}
     *              structure containing the reason.
     *
     * @param local_remote Indicates which peer made the request, one of:
     *		 <ul>
     *		 <li>PRO_ACTION_LOCAL</li>
     *		 <li>PRO_ACTION_REMOTE</li>
     *		 </ul>
     *
     * @param session_channel Indicates what was requested to close, one of:
     *		 <ul>
     *		 <li>PRO_ACTION_SESSION</li>
     *		 <li>PRO_ACTION_CHANNEL</li>
     *		 <li>PRO_ACTION_ABORT</li>
     *		 </ul>
     *           Note that in the last case, this is a demand, not a request.
     *
     *
     **/
    void (*pro_close_confirmation)(struct PROFILE_INSTANCE *pi,
				   char commit_or_rollback,
                                   struct diagnostic *error,
                                   char local_or_remote, 
                                   char session_or_channel);

    /**
     * @name pro_tuning_reset_indication
     *
     * Indicates that a local profile is getting ready to perform a
     * tuning reset, e.g., the TLS profile is getting ready to send the
     * &lt;ready&gt; element.
     * <p>
     * This is a FYI for the profile-instance -- there isn't an opportunity
     * to prevent the tuning reset from happening.
     *
     * @param pi A pointer to the profile-instance structure.
     *
     * @see bpc_tuning_reset_request
     **/
    void (*pro_tuning_reset_indication)(struct PROFILE_INSTANCE *pi);

    /**
     * @name pro_tuning_reset_confirmation
     *
     * Confirms the final disposition of a tuning reset.
     * <p>
     * If a tuning reset actually occurred, the profile-instance structure is
     * about
     * to be deallocated, so the profile should release any instance-specific
     * resources, e.g., referenced by <b>user_ptr1</b> and <b>user_ptr2</b>.
     *
     * @param pi A pointer to the profile-instance structure.
     *
     * @param commit_or_rollback Whether the tuning reset actually occurred, one of:
     *		 <ul>
     *		 <li>PRO_ACTION_SUCCESS</li>
     *		 <li>PRO_ACTION_FAILURE</li>
     *		 </ul>
     *
     * @see bpc_tuning_reset_complete
     **/
    void (*pro_tuning_reset_confirmation)(struct PROFILE_INSTANCE *pi,
					  char commit_or_rollback);

    /**
     * @name pro_frame_available
     *
     * Announces that a frame has arrived and is ready to be read,
     * if the profile is using the frame-oriented interface.
     * <p>
     * The profile should call
     * {@link bpc_query_frame bpc_query_frame}
     * or
     * {@link bpc_frame_read bpc_frame_read}
     * to fetch the frame, followed by
     * {@link bpc_frame_destroy bpc_frame_destroy}.
     *
     * @param pi A pointer to the profile-instance structure.
     *
     **/
    void (*pro_frame_available)(struct PROFILE_INSTANCE *pi);

    /**
     * @name pro_message_available
     *
     * Announces that a message has arrived and is ready to be read,
     * if the profile is using the message-oriented interface.
     * <p>
     * The profile should call
     * {@link bpc_query_message bpc_query_message}
     * to fetch a
     * linked-list of the frames making up the message, followed by
     * {@link bpc_frame_destroy bpc_frame_destroy}.
     * It may also be convenient to call
     * {@link bpc_frame_aggregate bpc_frame_aggregate}
     * to collapse the data from the linked-list into a character buffer.
     *
     * @param pi A pointer to the profile-instance structure.
     **/
    void (*pro_message_available)(struct PROFILE_INSTANCE *pi);

    /**
     * @name pro_window_full
     *
     * Announces that the receive buffer for a profile has filled,
     * but that a complete message has not yet been received.
     * <p>
     * The profile should do one of:
     * <ul>
     * <li>Call
     * {@link bpc_query_frame bpc_query_frame}
     * or
     * {@link bpc_frame_read bpc_frame_read}
     * to fetch the frame, followed by
     * {@link bpc_frame_destroy bpc_frame_destroy}.</li>
     *
     * <li>Call
     * {@link bpc_set_channel_window bpc_set_channel_window}
     * to increase the size of the receive buffer.</li>
     *
     * <li>Call
     * {@link bpc_error_allocate bpc_error_allocate}
     * to generate an error,
     * and then use
     * {@link bpc_send bpc_send}
     * to send it, pre-emptively
     * terminating the message and discarding any additional frames.</li>
     * </ul>
     *
     * @param pi A pointer to the profile-instance structure.
     **/
    void (*pro_window_full)(struct PROFILE_INSTANCE *pi);

    /**
     * The local peer requested the action.
     **/
#define PRO_ACTION_LOCAL		'L'

    /**
     * The remote peer requested the action.
     **/
#define PRO_ACTION_REMOTE		'R'

    /**
     * The request is to close the session.
     **/
#define PRO_ACTION_SESSION		'S'

    /**
     * The request is to close the channel.
     **/
#define PRO_ACTION_CHANNEL		'C'

    /**
     * The demands is to close the session.
     **/
#define PRO_ACTION_ABORT		'A'

    /**
     * The action occurred.
     **/
#define PRO_ACTION_SUCCESS		'+'

    /**
     * The action did not take place.
     **/
#define PRO_ACTION_FAILURE		'-'

//@}

    long user_long1;
    long user_long2;
    void *user_ptr1;
    void *user_ptr2;
};

typedef struct PROFILE_INSTANCE PROFILE_INSTANCE;

/**
 * @name Registration structure and functions
 **/
//@{

/**
 * A structure defined by the Profile API for registering a profile-module.
 * <p>
 * Before the
 * wrapper can invoke a profile-module's code, that profile-module
 * must be registered with the connection structure. If a single
 * profile-module implements multiple profiles (i.e., multiple URIs in the
 * greeting), then multiple profile registrations must be registered by that
 * profile-module.
 * <p>
 * This structure contains function pointers for all the callbacks invoked by
 * the wrapper. After a connection structure is created but before it is
 * started, all
 * profiles available to the application must be registered with the
 * connection structure.
 * A compiled-in profile-module may use the link to chain together
 * registrations at compile time. A dynamically-loaded profile-module
 * must export a function that returns a pointer to its profile-registration
 * structures.
 *
 * @field uri A pointer to the unique URI that identifies the profile
 *            being registered. This can be registered with IANA for
 *            standardized profiles. Note that each connection structure allows
 *            only one registration for each URI; that is, one
 *            cannot register multiple registrations with the same
 *            value for the URI. If both a client and server are
 *            implemented for the same URI, they must be implemented
 *            together.
 * 
 * @field next A pointer to another profile_registration. This allows
 *             for convenient compiled-in registrations and convenient
 *             dynamic libraries implementing multiple profiles. All
 *             profiles can be registered with a single call by making a
 *             linked-list of profile-registration structures.
 *             Multiple calls to register profiles are equally
 *             acceptable, or any mixture of the two techniques.
 *
 * @field initiator_modes A pointer to a list of modes separated by
 *                        commas in which this profile should have
 *                        {@link proreg_session_init proreg_session_init}
 *                        called. Each value
 *                        names one mode in which this profile will be
 *                        initialized, but in which this profile URI
 *                        will not be advertised in the greeting.
 *                        Possible values are <b>"plaintext"</b> and
 *                        <b>"encrypted"</b>.
 * 
 * @field listener_modes A pointer to a list of modes separated by
 *                       commas in which this profile should have
 *                       {@link proreg_session_init proreg_session_init}
 *                       called. Each value
 *                       names one mode in which this profile will be
 *                       initialized, and in which this profile URI
 *                       will be advertised in the greeting.
 *                       Possible values are <b>"plaintext"</b> and
 *                       <b>"encrypted"</b>.
 * 
 * @field full_messages Indicates whether the profile-instance is using
 *                      the frame-oriented or the message-oriented
 *                      interface.  In the latter case, if the receive
 *                      buffer fills up before a complete message is 
 *                      received, then
 *                      {@link pro_window_full pro_window_full}
 *                      being invoked.
 * 
 * @field thread_per_channel A flag indicating the threading behavior
 *                           behavior that the profile expects.  If true,
 *                           the profile expects or needs a
 *                           separate thread allocated for each channel
 *                           started under this profile; otherwise,
 *                           the profile does not expect its own thread
 *                           and may be run in any work thread.
 *
 * @field user_ptr A user-defined pointer.
 **/
struct PROFILE_REGISTRATION 
{
    struct BP_CONNECTION *allocator;            /* Reserved/private */
    char status;                                /* Reserved status flag */

    char *uri;
    struct PROFILE_REGISTRATION *next;
    char *initiator_modes;
    char *listener_modes;
    int full_messages;
    int thread_per_channel;

/**
 * @name Profile API functions
 *
 * Defines the function pointers for "outer" callbacks invoked by the wrapper. 
 **/
//@{

    /**
     * @name proreg_connection_init
     *
     * Invoked when the profile is registered with the wrapper.
     *
     * @param pr A pointer to the profile-registration structure.
     *
     * @param conn A pointer to the connection structure. 
     *
     * @return <b>NULL</b> if successful,
     * otherise a character pointer explaining the failure.
     **/
    char *(*proreg_connection_init)(struct PROFILE_REGISTRATION *pr,
				 struct BP_CONNECTION *conn);

    /**
     * @name proreg_connection_fin
     *
     * Invoked when the wrapper is destroyed.
     *
     * @param pr A pointer to the profile-registration structure.
     *
     * @param conn A pointer to the connection structure. 
     *
     * @return <b>NULL</b> if successful,
     * otherise a character pointer explaining the failure.
     **/
    char *(*proreg_connection_fin)(struct PROFILE_REGISTRATION *pr,
				struct BP_CONNECTION *conn);

    /**
     * @name proreg_session_init
     *
     * Invoked when a session is created (i.e., when a connection is
     * established or a tuning reset completes).
     *
     * @param pr A pointer to the profile-registration structure.
     *
     * @param conn A pointer to the connection structure. 
     *
     * @return <b>NULL</b> if successful,
     * otherise a character pointer explaining the failure.
     **/
    char *(*proreg_session_init)(struct PROFILE_REGISTRATION *pr,
				 struct BP_CONNECTION *conn);

    /**
     * @name proreg_session_fin
     *
     * Invoked when a session is destroyed (i.e., when a connection is
     * released or a tuning reset begins).
     *
     * @param pr A pointer to the profile-registration structure.
     *
     * @param conn A pointer to the connection structure. 
     *
     * @return <b>NULL</b> if successful,
     * otherise a character pointer explaining the failure.
     **/
    char *(*proreg_session_fin)(struct PROFILE_REGISTRATION *pr,
				struct BP_CONNECTION *conn);

    /**
     * @name proreg_greeting_notification
     *
     * Invoked when a greeting is received.
     *
     * @param pr A pointer to the profile-registration structure.
     *
     * @param conn A pointer to the connection structure.
     *
     * @param present_absent Indicates whether a URI corresponding to
     *			     the profile registration was present, one of:
     *		 <ul>
     *		 <li>PROFILE_PRESENT</li>
     *		 <li>PROFILE_ABSENT</li>
     *		 <li>GREETING_ERROR</li>
     *		 </ul>
     *           In the last case, an error was received instead of a greeting.
     *           Use {@link bp_greeting_error bp_greeting_error} to find out
     *		 what was in the &lt;error&gt; element.
     **/
    void (*proreg_greeting_notification)(struct PROFILE_REGISTRATION *pr,
                                         struct BP_CONNECTION *conn, 
                                         char present_absent);

    /**
     * The profile's URI was present in the greeting.
     **/
#define PROFILE_PRESENT         'P'

    /**
     * The profile's URI was absent from the greeting.
     **/
#define PROFILE_ABSENT          'A'

    /**
     * The remote peer sent an &lt;error&gt; instead of a greeting.
     **/
#define GREETING_ERROR          'E'

    /*
     * these are the "inner" callbacks...
     */

    /**
     * cf, {@link pro_start_indication pro_start_indication}
     **/
    void (*proreg_start_indication)(struct PROFILE_INSTANCE *,
                                    struct profile *);

    /**
     * cf, {@link pro_start_confirmation pro_start_confirmation}
     **/
    void (*proreg_start_confirmation)(void *client_data,
				      struct PROFILE_INSTANCE *, 
                                      struct profile *);

    /**
     * cf, {@link pro_close_indication pro_close_indication}
     **/
    void (*proreg_close_indication)(struct PROFILE_INSTANCE *,
				    struct diagnostic *,
                                    char,
                                    char);

    /**
     * cf, {@link pro_close_confirmation pro_close_confirmation}
     **/
    void (*proreg_close_confirmation)(struct PROFILE_INSTANCE *,
				      char,
                                      struct diagnostic *,
                                      char,
                                      char);

    /**
     * cf, {@link pro_tuning_reset_indication pro_tuning_reset_indication}
     **/
    void (*proreg_tuning_reset_indication)(struct PROFILE_INSTANCE *);

    /**
     * cf, {@link pro_tuning_reset_confirmation pro_tuning_reset_confirmation}
     **/
    void (*proreg_tuning_reset_confirmation)(struct PROFILE_INSTANCE *,
					     char);

    /**
     * cf, {@link pro_tuning_reset_handshake pro_tuning_reset_handshake}
     **/
    void (*proreg_tuning_reset_handshake)(struct PROFILE_INSTANCE *);

    /**
     * cf, {@link pro_frame_available pro_frame_available}
     **/
    void (*proreg_frame_available)(struct PROFILE_INSTANCE *);

    /**
     * cf, {@link pro_message_available pro_message_available}
     **/
    void (*proreg_message_available)(struct PROFILE_INSTANCE *);

    /**
     * cf, {@link pro_window_full pro_window_full}
     **/
    void (*proreg_window_full)(struct PROFILE_INSTANCE *);

//@}

    void *user_ptr;
};

typedef struct PROFILE_REGISTRATION  PROFILE_REGISTRATION;

//@Include: profile_loader.h

/**
 * Prototype for the well-known entry point required in a 
 * shared library which provides profiles.  Each such shared library 
 * must provide this function to return a list of the profile-registration
 * structures for all profiles in the library.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure that may be used for configuration purposes.
 *
 * @see load_beep_profiles
 * @see null_echo_Init
 * @see null_sink_Init
 * @see tls_profile_Init
 **/
typedef PROFILE_REGISTRATION *(*profile_shlib_init)(struct configobj *appconfig);

/**
 * Allocates a new profile-registration structure, filling-in everything but
 * the callback pointers.
 *
 * <p>
 * Note that the character pointer parameters are copied as well.
 * <p>
 * Use {@link bp_profile_registration_destroy bp_profile_registration_destroy}
 * to destroy the structure returned.
 **/
extern struct PROFILE_REGISTRATION*
bp_profile_registration_new(struct BP_CONNECTION *conn, 
                            char *uri,
                            char *initiator_modes, 
                            char *listener_modes,
                            int full_messages,
                            int thread_per_channel);
/**
 * Allocates a new profile-registration structure, copying everything from
 * another structure.
 * <p>
 * Note that the character pointer parameters are copied as well.
 * <p>
 * Use {@link bp_profile_registration_destroy bp_profile_registration_destroy}
 * to destroy the structure returned.
 *
 * @param conn A pointer to the connection structure, or <b>NULL</b>.
 *
 * @param pr A pointer to the profile_registration structure.
 **/
extern struct PROFILE_REGISTRATION*
bp_profile_registration_clone(struct BP_CONNECTION *conn,
                              struct PROFILE_REGISTRATION * pr);

/**
 * Deallocates a profile-registration structure returned by
 * {@link bp_profile_registration_new bp_profile_registration_new} or
 * {@link bp_profile_registration_clone bp_profile_registration_clone}
 *
 * @param conn A pointer to the connection structure, or <b>NULL</b>.
 *
 * @param pr A pointer to the profile-registration structure to be destroyed.
 **/
extern void bp_profile_registration_destroy(struct BP_CONNECTION *conn, 
                                            struct PROFILE_REGISTRATION *pr);
/**
 * Deallocates a linked-list of profile-registration structures.
 *
 * @param conn A pointer to the connection structure, or <b>NULL</b>.
 *
 * @param pr A pointer to the linked-list of profile-registration structures to be destroyed.
 **/
extern void
bp_profile_registration_chain_destroy(struct BP_CONNECTION *conn, 
                                      struct PROFILE_REGISTRATION *pr);
//@}

/**
 * @name Wrapper API functions
 **/
//@{

/**
 * @name Library Initialization
 **/
//@{

/**
 * Initializes the wrapper library.
 * <p>
 * This must be the first library function called.
 * The parameters initialize the memory management library,
 * which is also used by other libraries such as the hash table library, 
 * the linked list library, and so forth. The parameters are stored in global
 * variables, so it isn't possible to use different allocators for different
 * purposes.
 *
 * @param blw_fp_malloc A pointer to the malloc-compatible function the
 *                      wrapper should use for all its memory
 *                      allocation needs.
 *
 * @param blw_fp_free A pointer to the free-compatible function the
 *                    wrapper should use for all its memory allocation
 *                    needs.
 *
 * @return <b>BLW_OK</b> or <b>BLW_ERROR</b>.
 **/
extern int bp_library_init(blw_fp_malloc,
			    blw_fp_free);

/**
 * Terminates the wrapper library.
 *
 * This must be the last library function.
 *
 * @return <b>BLW_OK</b> or <b>BLW_ERROR</b>.
 **/ 
extern int bp_library_cleanup(void);

//@}

/**
 * @name Connection Management
 **/
//@{

/**
 * Binds a socket to a connection structure.
 *
 * @param socket The socket-descriptor.
 * 
 * @param mode A pointer to the initial connection mode,
 *             either <b>"plaintext"</b> or <b>"encrypted"</b>
 * 
 * @param ios TBD.
 *
 * @param log A pointer to the logging function, which is usually
 *            {@link log_line log_line}.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure that may be used for configuration purposes.
 *
 * @return A pointer to a {@link BP_CONNECTION connection} structure that
 *         represents the binding.
 *
 * @see tcp_bp_connect
 * @see tcp_bp_listen
 **/
extern struct BP_CONNECTION*
bp_connection_create(char *mode,
                     IO_STATE *ios,
                     void (*log)(int,int,char *,...),
                     struct configobj *appconfig);

/**
 * Registers a profile registration with a connection.
 * <p>
 * After making a copy of the profile-registration structure, it invokes
 * {@link pro_connection_init pro_connection_init} to see whether the profile
 * should be registered with this connection.
 *
 * @param conn A pointer to the connection structure. 
 *
 * @param pr A pointer to the profile-registration structure.
 *
 * @return On failure, a pointer to a {@link diagnostic diagnostic} structure
 *         explaining the reason
 *         (which should subsequently be destroyed by calling
 *         {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 **/
extern struct diagnostic* bp_profile_register(struct BP_CONNECTION *conn,
                                              struct PROFILE_REGISTRATION *pr);

/**
 * Starts the protocol engine after the desired profiles are registered
 * with a connection.
 *
 * @param conn A pointer to the connection structure. 
 *
 * @param role A character indicator of whether this is an initiating
 *           (<b>'I'</b>) or listening (<b>'L'</b>) connection.
 **/
extern void bp_start(struct BP_CONNECTION *conn, char role);

/**
 * Terminates all channels on the session.
 * <p>
 * Typically used prior to a call to
 * {@link bp_connection_destroy bp_connection_destroy}.
 *
 * @param conn A pointer to the connection structure.
 *
 * @return <b>BLW_OK</b> or <b>BLW_ERROR</b>.
 **/
extern int bp_shutdown(struct BP_CONNECTION *conn);

/**
 * Reaps the resources associated with the connection.
 * <p>
 * It is an error in logic to call this routine when any channels are open.
 * Note that this does not close the associated socket-descriptor.
 *
 * @param conn A pointer to the connection structure.
 **/
extern void bp_connection_destroy(struct BP_CONNECTION *conn);

/**
 * Returns the configuration object associated with the connection
 * (cf, {@link bp_connection_create bp_connection_create}).
 *  
 * @param conn A pointer to the connection structure. 
 *
 * @return The associated configuration object.
 **/
extern struct configobj* bp_get_config(struct BP_CONNECTION *conn);

/**
 * Makes a formatted log entry using the connection's logging function
 * (cf, {@link bp_connection_create bp_connection_create}).
 *
 * @param conn A pointer to the connection structure. 
 *
 * @param service The subsystem making the entry, one of:
 *                <ul>
 *                <li>LOG_MISC</li>
 *                <li>LOG_CORE</li>
 *                <li>LOG_WRAP</li>
 *                <li>LOG_PROF</li>
 *                </ul>
 *
 * @param severity The severity from <b>0</b> (lowest) to <b>7</b> (highest).
 *                 (Yes, this is inverted from the way <i>syslog</i> does it.)
 *
 * @param fmt... The format string and arguments.
 *
 * @see log_line
 **/
extern void bp_log(struct BP_CONNECTION *conn,
                   int service,
                   int severity,
                   char *fmt, ...);

//@}

/**
 * @name Session Management
 **/
//@{

/**
 * Waits until a &lt;greeting&gt; element is received from the remote peer.
 * <p>
 * Treat the return value as a constant, please. It goes away when the
 * connection structure is destroyed.
 *
 * @param conn A pointer to the connection structure. 
 *
 * @return A diagnostic structure if an &lt;error&gt; element was received
 *	   instead of a &lt;greeting&gt; element, otherwise <b>NULL</b>.
 **/
extern struct diagnostic* bp_wait_for_greeting(struct BP_CONNECTION *conn);

/**
 * If the remote peer sent an &lt;error&gt; element instead of a
 * &lt;greeting&gt; element, returns that.
 *
 * @param conn A pointer to the connection structure. 
 *
 * @return cf, {@link bp_wait_for_greeting bp_wait_for_greeting}.
 **/
extern struct diagnostic* bp_greeting_error(struct BP_CONNECTION *conn);

/**
 * Returns the <i>localize</i> attribute from the remote peer's
 * &lt;greeting&gt; element.
 * <p>
 * Treat the return value as a constant, please. It goes away when the
 * connection structure is destroyed.
 *
 * @param conn A pointer to the connection structure. 
 *
 * @return The remote peer's localization preference,
 *	   or <b>NULL</b> (if unknown or not present).
 **/
extern char* bp_server_localize(struct BP_CONNECTION *conn);

/**
 * Returns the <i>features</i> attribute from the remote peer's
 * &lt;greeting&gt; element.
 * <p>
 * Treat the return value as a constant, please. It goes away when the
 * connection structure is destroyed.
 *
 * @param conn A pointer to the connection structure. 
 *
 * @return The remote peer's features advertisement,
 *	   or <b>NULL</b> (if unknown or not present).
 **/
extern char* bp_features(struct BP_CONNECTION *conn);

/**
 * Returns the list of URIs presented in the remote peer's
 * &lt;greeting&gt; element,
 * as a NULL-terminated array of character pointers.
 * <p>
 * Treat the return value as a constant, please. It goes away when the
 * connection structure is destroyed.
 *
 * @param conn A pointer to the connection structure. 
 *
 * @return The remote peer's profiles advertisement,
 *	   or <b>NULL</b> (if unknown or not present).
 **/
extern char** bp_profiles_offered(struct BP_CONNECTION *conn);

/* doc++ note: i am NOT documenting this one, it should be internal... */
/*
 * Sets the privacy mode for the connection.
 *  
 * @param conn A pointer to the connection structure. 
 *
 * @param mode Either <b>"plaintext"</b> or <b>"encrypted"</b>.
 **/
extern void bp_mode_set(struct BP_CONNECTION *conn,
                        char *mode);

/**
 * Returns the list of URIs that the remote peer may ask to start
 * as a NULL-terminated array of character pointers.
 * <p>
 * The caller is responsible for freeing this array (but not each URI).
 *  
 * @param conn A pointer to the connection structure. 
 *
 * @return The list of URIs that the local peer is capable of "listening" for.
 **/
extern char** bp_profiles_avail_listen(struct BP_CONNECTION *conn);

/**
 * Returns the list of URIs that the local peer is capable of starting
 * as a NULL-terminated array of character pointers.
 * <p>
 * Note that you need to intersect this list with the return value of
 * {@link bp_profiles_offered bp_profiles_offered} to find out what
 * profiles you're <i>allowed</i> to start.
 * <p>
 * The caller is responsible for freeing this array (but not each URI).
 *  
 * @param conn A pointer to the connection structure. 
 *
 * @return The list of URIs that the local peer is capable of "initiating".
 **/
extern char** bp_profiles_avail_initiate(struct BP_CONNECTION *conn);

/**
 * Returns the list of URIs corresponding to the profiles registered
 * with this connection
 * as a NULL-terminated array of character pointers.
 * <p>
 * The caller is responsible for freeing this array.
 *  
 * @param conn A pointer to the connection structure. 
 *
 * @return The list of URIs.
 **/
extern char** bp_profiles_registered(struct BP_CONNECTION *conn);

/**
 * Returns the <i>serverName</i> attribute from the remote peer's first
 * successful start request.
 * <p>
 * Treat the return value as a constant, please. It goes away when the
 * connection structure is destroyed.
 *
 * @param conn A pointer to the connection structure. 
 *
 * @return The remote peer's server name,
 *	   or <b>NULL</b> (if unknown or not present).
 **/
extern char* bp_server_name(struct BP_CONNECTION *conn);

/**
 * Waits for the session to become quiescent.
 *
 * @param conn A pointer to the connection structure. 
 **/
extern void bp_wait_channel_0_state_quiescent(struct BP_CONNECTION *conn);

/* doc++ note: i am NOT documenting these two, they should be internal... */

/*
 * Accessor function to allow the application to associate an
 * arbitrary pointer with the connection structure.
 *  
 * @param conn A pointer to the connection structure.
 *
 * @param user_info An arbitrary pointer provided by the application.
 *
 * @see blu_session_info_set
 **/
extern void bp_set_session_info(struct BP_CONNECTION *conn,
                                void *user_info);

/*
 * Accessor function to allow the application to retrieve an associated
 * arbitrary pointer with the connection structure.
 *  
 * @param conn A pointer to the connection structure.
 *
 * @return The last pointer passed to
 * {@link bp_set_session_info bp_set_session_info}.
 *
 * @see blu_session_info_get
 **/
extern void* bp_get_session_info(struct BP_CONNECTION *conn);

/**
 * Accessor function to allow profiles to retrieve a pointer to the
 * transport state structure.
 *
 **/
extern IO_STATE* bp_get_iostate(struct BP_CONNECTION *conn);

//@}

/**
 * @name Channel Management
 **/
//@{

/**
 * Prototype for the callback to the application called when a start 
 * request has completed, regardless of its success.
 *
 * @param client_data A user-supplied pointer from
 *	{@link bp_start_request bp_start_request}
 *
 * @param inst A pointer to the channel-instance
 *           structure for the
 *           channel.  If the start request has failed,
 *           then points to a structure that will soon be destroyed
 *           (or may even be NULL).
 *
 * @param error If the start request has failed, this points to a
 *               {@link diagnostic diagnostic} structure containing the reason.
 *
 * @see bp_start_request
 **/
typedef void (*start_request_callback)(void *client_data,
                                       struct CHANNEL_INSTANCE *inst,
                                       struct diagnostic *error);

/**
 * Initiates a request to start a channel.
 * <p>
 * If the request can't be processed, a {@link diagnostic diagnostic} structure
 * is returned explaining the reason
 * (which should subsequently be destroyed by calling
 * {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 * <p>
 * Otherwise, later on the {@link (*start_request_callback) callback}
 * is invoked indicating whether the channel was started or not.
 * If so, the profile's {@link pro_start_confirmation pro_start_confirmation}
 * callback is invoked.
 * <I>bp_diagnostic_destroy</I>.
 * 
 * @param conn A pointer to the connection structure. 
 * 
 * @param channel_number The channel number to request or
 *                       <b>BLU_CHAN0_CHANO_DEFAULT</b>.
 *
 * @param message_number The message number to use on the request or
 *                       <b>BLU_CHAN0_MSGNO_DEFAULT</b>.
 *
 * @param po A linked list of {@link profile profile} structures.
 *
 * @param server_name The value to use for the <i>serverName</i> attribute
 *                    of the &lt;start&gt; element, or <b>NULL</b>.
 *
 * @param app_start_request_callback A callback to be initiated when
 *        the outcome is known.
 *
 * @param client_data A user-supplied pointer.
 *
 * @return On failure, a pointer to a {@link diagnostic diagnostic} structure
 *         explaining the reason (which should subsequently
 *         be destroyed by calling
 *         {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 **/
extern struct diagnostic*
bp_start_request(struct BP_CONNECTION *conn,
                 long channel_number,
                 long message_number,
                 struct profile *po,
                 char *server_name,
                 start_request_callback app_sr_callback,
                 void *client_data);

#define BLU_CHAN0_CHANO_DEFAULT (-1)
#define BLU_CHAN0_MSGNO_DEFAULT (-1)

/**
 * Invoked by the {@link pro_start_indication pro_start_indication} callback
 * to indicate the disposition of a request to start a channel.
 * <p>
 * To accept, the <i>diag</i> parameter should be <b>NULL</b>; otherwise,
 * this parameter should explain why the request was declined.
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param po A pointer to the {@link profile profile}
 * structure, which is identical in every way to the <i>profile</i>
 * parameter provided to the {@link pro_start_indication pro_start_indication}
 * callback, with the exception that the <i>piggyback</i> and
 * <i>piggyback_length</i> fields may be different.
 *
 * @param diag If declining, a pointer to a {@link diagnostic diagnostic}
 *	       structure explaining the reason (which should subsequently
 *             be destroyed by calling
 *             {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 *
 * @see pro_start_indication
 **/

extern void bpc_start_response(struct CHANNEL_INSTANCE *inst,
                                struct profile *po,
                                struct diagnostic *diag);

/**
 * Prototype for the callback to the application called when a close
 * request has completed, regardless of its success.
 *
 * @param client_data A user-supplied pointer from 
 *	{@link bpc_close_request bpc_close_request}
 *
 * @param inst A pointer to the channel-instance
 *           structure for the
 *           channel.  If the close request has succeeded,
 *           then this points to a structutre that 
 *           will soon be destroyed.
 *
 * @param error If the close request has failed, this points to a
 *               {@link diagnostic diagnostic} structure containing the reason.
 *
 * @see bpc_close_request
 **/
typedef void (*close_request_callback)(void *client_data,
                                       struct CHANNEL_INSTANCE *inst,
                                       struct diagnostic *error);

/**
 * Initiates a request to close a channel.
 * <p>
 * If the request can't be processed, a {@link diagnostic diagnostic} structure
 * is returned explaining the reason
 * (which should subsequently be destroyed by calling
 * {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 * <p>
 * Otherwise, the profile's {@link pro_close_indication pro_close_indication}
 * callback is invoked to ask permission.
 * If denied, the {@link (*close_request_callback) callback} and
 * the profile's {@link pro_close_confirmation pro_close_confirmation}
 * callback is invoked to indicate this.
 * <p>
 * Otherwise,
 * later on the {@link (*close_request_callback) callback} and
 * the profile's {@link pro_close_confirmation pro_close_confirmation}
 * callback will be invoked indicating whether the channel was closed or not.
 *
 * @param channel A pointer to the channel-instance.
 *
 * @param message_number The message number to use on the request or
 *                       <b>BLU_CHAN0_MSGNO_DEFAULT</b>.
 *
 * @param message_number Message number on channel 0 to send.  -1
 * means use next.
 * 
 * @param code The value to use for the <i>code</i> attribute of the
 *             &lt;close&gt; element. Typically, <b>200</b> for "OK".
 * Consult \URL[Section 8 of RFC 3080]{http://www.beepcore.org/beepcore/docs/rfc3080.jsp#reply-codes}
 * for an (incomplete) list.
 *
 * @param language The value to use for the <i>xml:lang</i> attribute of the
 *                 &lt;close&gt; element.
 *
 * @param message A textual message corresponding to the <i>code</i> parameter.
 *
 * @param app_closet_request_callback A callback to be initiated when
 *        the outcome is known.
 *
 * @param client_data A user-supplied pointer.
 * 
 * @return On failure, a pointer to a {@link diagnostic diagnostic} structure
 *         explaining the reason (which should subsequently
 *         be destroyed by calling
 *         {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 */
extern struct diagnostic*
bpc_close_request(struct CHANNEL_INSTANCE *inst,
                  long message_number,
                  long code,
                  char *language,
                  char *message,
                  close_request_callback app_cr_callback,
                  void *client_data);

/**
 * Invoked by the {@link pro_close_indication pro_close_indication} callback
 * to indicate the disposition of a request to close a channel.
 * <p>
 * To accept, the <i>diag</i> parameter should be <b>NULL</b>; otherwise,
 * this parameter should explain why the request was declined.
 * <p>
 * If the request is accepted, the profile should wait for the
 * {@link pro_close_confirmation pro_close_confirmation} callback to be
 * made to indicate the final disposition of the request. Until that time,
 * the profile should stop sending MSGs,
 * but should continue processing any traffic it receives.
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param diag If declining, a pointer to a {@link diagnostic diagnostic}
 *	       structure explaining the reason (which should subsequently
 *             be destroyed by calling
 *             {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 *
 * @see pro_close_indication
 **/
extern void bpc_close_response(struct CHANNEL_INSTANCE *inst,
                                struct diagnostic *diag);

/* doc++ note: i am NOT documenting these two, they should be internal... */

/*
 * Accessor function to allow the application or profile to retrieve an 
 * arbitrary pointer stored in the channel instace.
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @return The most recent pointer passed to 
 * {@link bpc_set_channel_info bpc_set_channel_info} for this channel.
 **/
extern void* bpc_get_channel_info(struct CHANNEL_INSTANCE *inst);

/*
 * Accessor function to allow the application or profile to store an 
 * arbitrary pointer with the channel instace.
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param data An arbitrary pointer provided by the application.
 **/
extern void bpc_set_channel_info(struct CHANNEL_INSTANCE *inst,
				  void *data);

/**
 * Sets the receive window size for a channel.
 *
 * <p>
 * If the size requested is <b>-1</b>, then the receive window size isn't
 * updated.
 * (This allows you to find out the current receive window size without
 * changing it.)
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param size The receive window size, in octets, or <b>-1</b>.
 *
 * @return The previous receive window size, in octets.
 **/
extern long bpc_set_channel_window(struct CHANNEL_INSTANCE *inst,
				    long size);

/**
 * Waits for a channel to become quiescent.
 *
 * @param inst A pointer to the channel-instance structure.
 **/
extern void bpc_wait_state_quiescent(struct CHANNEL_INSTANCE *inst);

/**
 * Invokes the {@link pro_tuning_reset_indication pro_tuning_reset_indication}
 * callback for all channels.
 * <p>
 * After the callbacks are invoked, this routine waits until all channels are
 * quiescent.
 * <p>
 * This routine is called after a tuning profile has been started,
 * and immediately before it's ready to send the MSG triggering the lower-layer
 * negotiations.
 *
 * @param inst A pointer to the channel-instance structure associated with
 *             the tuning profile.
 **/
extern void bpc_tuning_reset_request(struct CHANNEL_INSTANCE *inst);

/**
 * Indicates that a tuning reset has either completed or aborted,
 * and invokes the
 * {@link pro_tuning_reset_confirmation pro_tuning_reset_confirmation}
 * callback for all channels.
 *
 * @param inst A pointer to the channel-instance structure associated with
 *             the tuning profile.
 *
 * @param commit_or_rollback Whether the tuning reset actually occurred, one of:
 *                           <ul>
 *                           <li>PRO_ACTION_SUCCESS</li>
 *                           <li>PRO_ACTION_FAILURE</li>
 *                           </ul>
 *
 * @param mode Either <b>"plaintext"</b> or <b>"encrypted"</b>.
 **/
extern void bpc_tuning_reset_complete(struct CHANNEL_INSTANCE *inst,
                                      char commit,
                                      char *mode);

//@}

/**
 * @name Diagnostic Management
 **/
//@{

/**
 * Allocates a {@link diagnostic diagnostic} structure.
 * <p>
 * Use {@link bp_diagnostic_destroy bp_diagnostic_destroy} to deallocate
 * the structure returned.
 *
 * @param conn A pointer to the connection structure, or <b>NULL</b>. 
 *
 * @param code The numeric reason.
 * Consult \URL[Section 8 of RFC 3080]{http://www.beepcore.org/beepcore/docs/rfc3080.jsp#reply-codes}
 * for an (incomplete) list.
 *
 * @param language The localization language used by the <i>message</i>
 *                 parameter.
 *
 * @param fmt... A textual message, expressed as a format string and arguments,
 *               corresponding to the <i>code</i> parameter.
 *
 * @return The diagnostic structure that is allocated, or <b>NULL</b>.
 **/
extern struct diagnostic* bp_diagnostic_new(struct BP_CONNECTION *conn,
                                            int code,
                                            char *language,
                                            char *fmt, ...);

/**
 * Deallocates a {@link diagnostic diagnostic} structure.
 *
 * @param conn A pointer to the connection structure, or <b>NULL</b>. 
 *
 * @param d The structure to free.
 **/
extern void bp_diagnostic_destroy(struct BP_CONNECTION *conn,
                                  struct diagnostic *d);

//@}

//@}

/**
 * @name Channel Instance structure and functions
 **/
//@{

/**
 * A structure defined by the profile API for managing a channel-instance.
 * <p>
 * This is an opaque structure, and is the dual of the
 * {@link PROFILE_INSTANCE profile-instance} structure.
 * <p>
 * Unless otherwise noted, all fields should be considered read-only by the
 * profile.
 *  
 * @field  conn A pointer to the associated {@link BP_CONNECTION connection}
 *              structure.
 *
 * @field channel_number The associated channel number.
 *
 * @field profile_registration A pointer to the
 *                        {@link PROFILE_REGISTRATION profile-registration}
 *                        structure used to instantiate this channel.
 *
 * @field profile         A pointer to the associated
 *                        {@link PROFILE_INSTANCE profile-instance}
 *                        structure.
 *
 * @field status The channel's current status, one of:
 *               <ul>
 *               <li>INST_STATUS_STARTING</li>
 *               <li>INST_STATUS_CLOSING</li>
 *               <li>INST_STATUS_TUNING</li>
 *               <li>INST_STATUS_EXIT</li>
 *               <li>INST_STATUS_RUNNING</li>
 *               </ul>
 *
 * @field outbound A pointer to the chan0_msg structure sent for 
 *                 for a pending start or close channel request.
 *                 (This structure isn't user-documented, don't ask.)
 *
 * @field inbound A pointer to the chan0_msg structure received for 
 *                        for a pending start or close channel request.
 *                        (This structure isn't user-documented, don't ask;
 *                        however, see {@link pro_start_indication pro_start_indication}
 *                        for the exception that proves the rule, i.e.,
 *                        to see the one place you may need to access this structure.)
 *
 * @field app_sr_callback A pointer to a callback in the 
 *                        aplication which is called when a start request
 *                        has been completed.   This is provided as an 
 *                        argument to
 *                        {@link bp_start_request bp_start_request}.
 *
 * @field app_cr_callback A pointer to a callback in the 
 *                        aplication which is called when a close request
 *                        has been completed.   This is provided as an 
 *                        argument to
 *                        {@link bpc_close_request bpc_close_request}.
 *
 * @field cb_client_data A user-supplied pointer given as an argument to either
 *                       {@link bp_start_request bp_start_request} or
 *                       {@link bpc_close_request bpc_close_request}.
 *
 * @field diagnostic      A pointer to a {@link diagnostic diagnostic}
 *                        structure supplied when the callback calls either
 * {@link bpc_start_response bpc_start_response} or
 * {@link bpc_close_response bpc_close_response}.
 *
 * @field commit          Keeps track of success/failure when processing a
 *                        multistage close request.
 *
 * @field user_sem1       A semaphore used by the wrapper.
 *
 * @field user_sem2       A semaphore used by the wrapper.
 *
 * @field tid             Thread information used by the wrapper.
 *
 * @field lmem            Memory information used by the wrapper.
 **/
struct CHANNEL_INSTANCE
{
    struct BP_CONNECTION *conn;
    long channel_number;
    void* channel_info;
    struct PROFILE_REGISTRATION *profile_registration;
    struct PROFILE_INSTANCE *profile;
    char status; /* ' ', 'S'tarting, 'C'losing, 'T'uning, etc */
    struct chan0_msg *outbound;
    struct chan0_msg *inbound;
    start_request_callback app_sr_callback;
    close_request_callback app_cr_callback;
    void *cb_client_data;
    struct diagnostic *diagnostic;
    char commit;
    bp_queue* pending_send;
    bp_queue* pending_event;

#ifdef WIN32
    HANDLE user_sem1;
    HANDLE user_sem2;
    HANDLE tid;
#else
    sem_t user_sem1;
    sem_t user_sem2;
    THREAD_T tid;
#endif

    bp_slist *lmem;
};

typedef struct CHANNEL_INSTANCE CHANNEL_INSTANCE;

/**
 * @name Buffer Management
 **/
//@{

/**
 * Allocates a frame of the specified size.
 * <p>
 * Use {@link bpc_frame_destroy bpc_frame_destroy} to deallocate
 * the frame returned.
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param size The size of the payload to be allocated.
 *
 * @return The frame that is allocated, or <b>NULL</b>.
 **/
extern struct frame* bpc_frame_allocate(struct CHANNEL_INSTANCE *inst,
                                        size_t size);

/**
 * Allocates a buffer of the specified size.
 * <p>
 * Use {@link bpc_buffer_destroy bpc_buffer_destroy} to deallocate
 * the buffer returned, but only if it has not been used as a parameter to
 * {@link bpc_send bpc_send}.
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param size The size of the buffer to be allocated.
 *
 * @return The buffer that is allocated, or <b>NULL</b>.
 **/
extern char* bpc_buffer_allocate(struct CHANNEL_INSTANCE *inst,
                                 size_t size);

/**
 * Allocates a buffer containing an &lt;error&gt; element.
 * <p>
 * Use {@link bpc_buffer_destroy bpc_buffer_destroy} to deallocate
 * the buffer returned, but only if it has not been used as a parameter to
 * {@link bpc_send bpc_send}.
 * (If the channel instance is <b>NULL</b>, then use {@link lib_free lib_free}
 * to free it.)
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param code The value to use for the <i>code</i> attribute of the
 *		&lt;close&gt; element.
 * Consult \URL[Section 8 of RFC 3080]{http://www.beepcore.org/beepcore/docs/rfc3080.jsp#reply-codes}
 * for an (incomplete) list.
 *
 * @param language The value to use for the <i>xml:lang</i> attribute of the
 *		&lt;error&gt; element.
 *
 * @param message A textual message corresponding to the <i>code</i> parameter.
 *
 * @return The buffer that is allocated, or <b>NULL</b>.
 **/
extern char* bpc_error_allocate(struct CHANNEL_INSTANCE *inst,
                                int code,
                                char *language,
                                char *message);

/**
 * Allocates a buffer by aggregating the payloads of all the frames in a
 * message.
 * <p>
 * Use {@link bpc_buffer_destroy bpc_buffer_destroy} to deallocate
 * the buffer returned, but only if it has not been used as a parameter to
 * {@link bpc_send bpc_send}.
 * <p>
 * This routine makes a copy of the payloads, and does not affect either
 * the original frames nor the  receive window size.
 * 
 * @param inst A pointer to the channel-instance structure.
 *
 * @param in A pointer to the first frame in a message.
 *
 * @param buffer <b>(out)</b> A character pointer updated to point to the
 *               newly-allocated  buffer.
 *
 * @return int The size of the buffer, or <b>-1</b> on failure.
 **/ 
extern int bpc_frame_aggregate(struct CHANNEL_INSTANCE *inst,
                               struct frame *in,
                               char **buffer);

/**
 * Deallocates a buffer returned by 
 * {@link bpc_buffer_allocate bpc_buffer_allocate},
 * {@link bpc_error_allocate bpc_error_allocate}, or
 * {@link bpc_frame_aggregate bpc_frame_aggregate}.
 * <p>
 * Do not call this routine if the buffer was used as a parameter to
 * {@link bpc_send bpc_send}.
 *
 * @param inst A pointer to the channel-instance structure.
 * 
 * @param buffer A pointer to the buffer to be reaped.
 **/
extern void bpc_buffer_destroy(struct CHANNEL_INSTANCE *inst,
                               char *buffer);

/**
 * Deallocates a frame returned by
 * {@link bpc_frame_allocate bpc_frame_allocate},
 * {@link bpc_frame_read bpc_frame_read}, or
 * {@link bpc_query_frame bpc_query_frame}.
 *
 * <p>
 * It is important to call this routine as soon as practical,
 * since it updates the receive window size.
 * <p>
 * Note that unlike {@link bpc_message_destroy bpc_message_destroy},
 * this routine reaps exactly one frame (i.e., if the frame is the head of
 * a linked list, it doesn't reap the entire chain.)
 * 
 * @param inst A pointer to the channel-instance structure.
 * 
 * @param f A pointer to the frame to be reaped.
 **/
extern void bpc_frame_destroy(struct CHANNEL_INSTANCE *inst,
                              struct frame *f);

/**
 * Deallocates a frame returned by
 * {@link bpc_query bpc_query}, or
 * {@link bpc_query_message bpc_query_message},
 *
 * <p>
 * It is important to call this routine as soon as practical,
 * since it updates the receive window size.
 * <p>
 * Note that unlike {@link bpc_frame_destroy bpc_frame_destroy},
 * this routine reaps every frame in the linked list.
 * 
 * @param inst A pointer to the channel-instance structure.
 * 
 * @param f A pointer to the first frame of the message to be reaped.
 **/
extern void bpc_message_destroy(struct CHANNEL_INSTANCE *inst,
                                struct frame *f);

//@}

/**
 * @name Frame Management
 **/
//@{

/**
 * Sends a buffer allocated by
 * {@link bpc_buffer_allocate bpc_buffer_allocate},
 * {@link bpc_error_allocate bpc_error_allocate}, or
 * {@link bpc_frame_aggregate bpc_frame_aggregate}.
 * 
 * @param inst A pointer to the channel-instance structure.
 *
 * @param message_type Indicates the message type, one of:
 *                <ul>
 *                <li>BLU_FRAME_TYPE_MSG</li>
 *                <li>BLU_FRAME_TYPE_RPY</li>
 *                <li>BLU_FRAME_TYPE_ANS</li>
 *                <li>BLU_FRAME_TYPE_NUL</li>
 *                <li>BLU_FRAME_TYPE_ERR</li>
 *                </ul>
 * 
 * @param message_number Indicates the message number;
 *		if <i>message_type</i> is <b>BLU_FRAME_TYPE_MSG</b>,
 *		then <b>BLU_FRAME_MSGNO_UNUSED</b> may be used instead.
 * 
 * @param answer_number Indicates the answer number;
 *		if <i>message_type</i> isn't <b>BLU_FRAME_TYPE_ANS</b>,
 *		then use <b>BLU_FRAME_IGNORE_ANSNO</b>.
 * 
 * @param more Indicates if a complete message is being sent, one of:
 *                <ul>
 *                <li>BLU_FRAME_COMPLETE</li>
 *                <li>BLU_FRAME_PARTIAL</li>
 *                </ul>
 *
 * @param buffer A pointer to the buffer.
 **/
extern void bpc_send(struct CHANNEL_INSTANCE *inst,
                     char message_type,
                     long message_number,
                     long answer_number,
                     char more,
                     char *buffer,
                     long length);

#define BLU_FRAME_COMPLETE      BLU_QUERY_COMPLETE
#define BLU_FRAME_PARTIAL       BLU_QUERY_PARTIAL
#define BLU_FRAME_MSGNO_UNUSED (-1)
#define BLU_FRAME_IGNORE_ANSNO  BLU_QUERY_ANYANS

/**
 * Returns the first frame available on a channel, if any.
 * <p>
 * Use {@link bpc_frame_destroy bpc_frame_destroy} to deallocate
 * the frame returned.
 *
 * @param inst A pointer to the channel-instance structure.
 * 
 * @return The oldest frame available, or <b>NULL</b>.
 **/
extern struct frame* bpc_frame_read(struct CHANNEL_INSTANCE *inst);

/**
 * Returns the first (or all) frames available on a channel that matches
 * the query parameters, if any.
 * <p>
 * Use {@link bpc_message_destroy bpc_message_destroy} to deallocate
 * the frame(s) returned.
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param message_type Either <b>BLU_QUERY_ANYTYPE</b>, or one of:
 *                <ul>
 *                <li>BLU_FRAME_TYPE_MSG</li>
 *                <li>BLU_FRAME_TYPE_RPY</li>
 *                <li>BLU_FRAME_TYPE_ANS</li>
 *                <li>BLU_FRAME_TYPE_NUL</li>
 *                <li>BLU_FRAME_TYPE_ERR</li>
 *                </ul>
 *
 * @param message_number Either a specific message number,
 *                       or <b>BLU_QUERY_ANYMSG</b>.
 *
 * @param answer_number Either a specific answer number,
 *                      or <b>BLU_QUERY_ANYANS</b>.
 *
 * @param more Either <b>BLU_QUERY_PARTIAL</b>, to match any frame; or,
 *             <b>BLU_QUERY_COMPLETE</b> to match all frames contained in a
 *             complete message, but only if  a complete message is available
 *
 * @return The old frame matching the query, or <b>NULL</b>.
 **/
extern struct frame* bpc_query(struct CHANNEL_INSTANCE *inst,
                               char message_type,
                               long message_number,
                               long answer_number,
                               char more);

/**
 * Returns the first frame available on a channel that matches the query
 * parameters, if any.
 *
 * <p>
 * This routine simply calls {@link bpc_query bpc_query} with the
 * <i>more</i> parameter set to <b>BLU_QUERY_PARTIAL</b>.
 **/
extern struct frame* bpc_query_frame(struct CHANNEL_INSTANCE *inst,
                                     char message_type,
                                     long message_number,
                                     long answer_number);

/**
 * Returns the first complete message available on a channel that matches the
 * query parameters, if any.
 *
 * <p>
 * This routine simply calls {@link bpc_query bpc_query} with the
 * <i>more</i> parameter set to <b>BLU_QUERY_COMPLETE</b>.
 **/
extern struct frame* bpc_query_message(struct CHANNEL_INSTANCE *inst,
                                       char message_type,
                                       long message_number,
                                       long answer_number);

/**
 * Parses a frame containing an &lt;error&gt; element.
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param f A pointer to a frame.
 *
 * @return A {@link diagnostic diagnostic} structure
 *         (which should subsequently be destroyed by calling
 *         {@link bp_diagnostic_destroy bp_diagnostic_destroy}).
 **/
extern struct diagnostic* bpc_parse_error(struct CHANNEL_INSTANCE *inst,
                                          struct frame *f);

/**
 * Gets the frame associated with the buffer created by
 * {@link bpc_buffer_allocate bpc_buffer_allocate},
 * {@link bpc_error_allocate bpc_error_allocate}, or
 * {@link bpc_frame_aggregate bpc_frame_aggregate}.
 * <p>
 * Do not call this routine if the buffer was used as a parameter to
 * {@link bpc_send bpc_send}.
 *
 * @param inst A pointer to the channel-instance structure.
 *
 * @param f A pointer to buffer.
 *
 * @return A pointer to frame structure which may be destroyed by
 *         {@link bpc_frame_destroy bpc_frame_destroy},
 *         but ONLY if you're haven't called
 *         {@link bpc_buffer_destroy bpc_buffer_destroy}).
 **/
extern struct frame* bpc_b2f (CHANNEL_INSTANCE* inst, char* buffer);

//@}

//@}

/*
 * Wrapper related constants.
 */
#define BLW_CZ_ACTION_QSIZE            64
#define BLW_CHAN0_HELPERTHREAD         -2


#define BLW_PROF_RESPONSE_PEND	      'P'
#define BLW_PROF_RESPONSE_DONE	      'C'



/*
 * Wrapper related return codes.
 */
#define BLW_OK                         0
#define BLW_ERROR                    (-1)

#define BLW_ALLOC_FAIL               421
#define BLW_BEEP_LIBRARY_ERROR       422
#define	BLW_CHANNEL_BUSY             423
#define BLW_REGISTER_INCOMPLETE      501
#define BLW_REGISTER_DUPURI          502
#define BLW_REGISTER_INITFAIL        503
#define BLW_CHANNEL_NOT_OPEN         504
#define BLW_START_PROF_NOT_REG       505

#if defined(__cplusplus)
}
#endif

#endif
