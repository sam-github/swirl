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
 * $Id: bp_wrapper_private.h,v 1.11 2002/04/04 05:15:42 huston Exp $
 *
 * bp_wrapper_private.h
 *
 * Functionality for the wrapper library for the BEEP core library.
 * Provided here are the interfaces to the application for managing a
 * BEEP connection.  The wrapper provides an abstration layer for
 * memory managememt to the underlying routines and may also 
 * implement policies on profile stratup enhancing the default 
 * behaviour as described in the RFC.
 */
#ifndef BP_WRAPPER_PRIVATE_H
#define BP_WRAPPER_PRIVATE_H 1

#include "../transport/bp_fiostate.h"
#include "bp_wrapper.h"

#ifdef WIN32
#include <stddef.h>
#else
#define offsetof(TYPE, MEMBER) ((int) &((TYPE*)0)->MEMBER)
#endif

#define GET_WRAPPER(_conn) ((WRAPPER*)((char*)(_conn) - offsetof(WRAPPER, conn)))

typedef struct _wrapper WRAPPER;
typedef struct _profile_response PROFILE_RESPONSE;

/**
 * The wrapper instance structure is associated with a single socket for the
 * lifetime of that socket. The code that manages the wrapper instance
 * structure is called the wrapper. Its responsibilities include mapping of
 * channel numbers to profile instances, mapping of URIs to profile
 * registrations, and invoking profiles at the appropriate time as messages
 * arrive on the socket. It also manages input/output threads, profile
 * threads, and locks to prevent conflicts. It manages tuning resets and
 * graceful termination of sessions and controls.
 * Finally, the wrapper indirectly manages any information
 * that is inter-channel in nature, such as the SASL identity, the TLS
 * encryption keys, and any other information a profile might wish to make
 * available to other profiles.
 *
 * <p>This structure contains a pointer to a linked list of profile
 * registrations, a linked list of profile instances, a socket, TLS
 * information for the socket, SASL identity information, a session structure,
 * a semaphore to serialize access to the session, a linked list of profile
 * instances, a "current mode", a "close session" callback, a set of
 * outstanding channel-zero requests and replies, and so on. The profile
 * implementation author will rarely (if ever) need to examine this structure.
 * It should be treated as an opaque structure, accessed only via the defined
 * API.
 *
 **/
struct _wrapper 
{
    BP_CONNECTION conn;
    IO_STATE *iostate;
    sem_t lock;
    sem_t core_lock;
    sem_t callback_lock;
    struct session *session;
    int channel_ref_count;

    bp_slist *profile_registration;
    bp_slist *profile_registration_active;

    bp_queue *pending_chan0_actions;
    struct chan0_msg * remote_greeting; 
    char **offered_profiles; 
    bp_slist *tuning_instance;

    PROFILE_RESPONSE ** profile_responses;
    unsigned long profile_responses_seq;
    BP_HASH_TABLE *error_remembered;
    bp_slist *lmem;
    int qsize;
    void (*log)(int,int,char *,...);
    
    struct configobj *appconfig;

    void *sessionContextTable;

    void *externalSessionReference;
    void* session_info;
    void* reader_writer;
};

typedef struct {
    void* s;
    long c;
    int i;
} PENDING_EVENT;

/**
 * Wrapper function called when a start indication message is received.
 * The profiles listed in the profile section are tried in turn until
 * a profile can successfully be started.  If no profiles can be started
 * then an error is returned.  In starting a profile the 
 * <i>pro_start_indication</i> function is called; if it succeeds then 
 * the channel is instantiated, and the profile element is returned to the
 * requesting peer.
 *
 * <P> Furture implementations might implement policy here for what profiles
 * may be started given current authentication, encryption level, or
 * machine loading for example.
 *
 * @param wrap A pointer to the wrapper structrue.
 *
 * @param request A pointer to the chan0_msg structure. 
 **/
extern void blw_start_indication(WRAPPER *wrap, struct chan0_msg *request);

/**
 * Confirms that the given channel has been successfully started.
 * <I>blw_start_confirmation</I> is invoked after the start request
 * callback has returned. If the &lt;start&gt; is refused by the remote peer
 * via the return of an &lt;error&gt; element, <I>blw_start_confirmation</I>
 * is not called; in that case only the start request callback is
 * called. On a channel started by the local peer, this is the first
 * function invoked on the new profile instance.  Both the original
 * request and the confirmation are passed to this function, as well
 * as the client data pointer from the original
 * <I>blw_start_request</I> call.
 * <p> DOC BUG: PROTOTYPE IS INCORRECT.
 *
 * @param wrap A pointer to the wrapper structure.
 *
 * @param confirm A pointer to the chan0_msg structure for the confirmation.
 *
 * @see blw_start_request
 **/
extern void blw_start_confirmation(WRAPPER *wrap, struct chan0_msg *confirm);

/**
 * Handle the receipt of a greeting confirmation, which is the first BEEP
 * message received. 
 * <p> DOCUMENTATION BUG: HUH? WHY IS THIS EXPOSED? AND WHY ARE THE ARGS DOCED WRONG?
 *
 * @param inst Pointer to the wrapper instance on which the tuning reset
 *             request is made.  
 *
 * @param notify A pointer to the notification chan0_msg request structure.
 *
 **/
extern void blw_greeting_notification(WRAPPER *wrap,
                                      struct chan0_msg *notify);

/**
 * Control loop for newly created wrappers binding the transport and
 * wrapper layers.  This is a container function for the lower level
 * routines <i>blw_initiator_start</i> and <i>blw_listener_start</i>.
 * 
 * @param wrap A pointer to the wrapper structure
 *
 * @param pi 'L' for listening mode or 'I' for initiating mode
 * BUG: Why two IL flags, in create and start? Why "pi" for variable?
 **/
extern void blw_started(WRAPPER *wrap,char pi);
/**
 * Lower level routine that runs the wrapper in a thread and handles 
 * I/O on the connection/handle bound to the wrapper.
 * <p> BUG: Err, now we have three places we specify I or L.
 *
 * @param data Actually a pointer to the wrapper structure, this is a
 *             void for the puroposes of more generic thread queueing.
 **/
#ifdef WIN32
DWORD WINAPI blw_initiator_start(void *data);
#else
extern void *blw_initiator_start(void *data);
#endif

/**
 * Lower level routine that will execute as a listener in the thread and
 * spawn a wrapper for each connection.
 * <p> BUG: Err, now we have three places we specify I or L.
 *
 * @param data Actually a pointer to the wrapper structure, this is a
 *             void for the puroposes of more generic thread queueing.
 **/
#ifdef WIN32
DWORD WINAPI blw_listener_start(void *data);
#else
extern void *blw_listener_start(void *data);
#endif

/**
 * Maps the specified channel number to a wrapper channel instance. 
 *
 * @param wrap A pointer to the wrapper structure.
 *
 * @param channel_number The channel number to be mapped.
 *
 * @return A pointer to the channel instance structure, which contains NULL if 
 *         the given channel number is not associated with a profile instance. 
 *         This would be true of a closed channel, channel zero, or a channel 
 *         still waiting for the start confirmation.
 **/
extern CHANNEL_INSTANCE *blw_map_channel_number(WRAPPER *wrap,
                                                long channel_number);

/*
 * blw_malloc
 * blw_free
 *
 * Memory management routines provided as a convenience for 
 * the profile implementor.  These routines track memory 
 * allocated on a per wrapper basis and will free any momory
 * not freed by the allocator when the wrapper/session is closed.
 */

/**
 * Returns a pointer to a piece of memory at least "size" bytes long, using 
 * version of "malloc" that the wrapper is configured to use. It also 
 * tracks the space that will be freed when the wrapper is destroyed if not 
 * freed earlier by <I>blw_free</I>. 
 *
 * @param wrap A pointer to the wrapper structure.
 * 
 * @param size The size in bytes of the memory to be allocated. 
 **/
extern void * blw_malloc(WRAPPER *wrap, size_t size);

/** 
 * Deallocates a piece of memory allocated by <I>blw_malloc</I>. 
 *
 * @param wrap A pointer to the wrapper structure for which memory is to be  
 *             deallocated. 
 *
 * @param ptr A pointer to memory previously allocated by blw_malloc and
 *            not yet freed.
 **/
extern void blw_free(WRAPPER *wrap, void *ptr);

/**
 * Constructor for a channel 0 message suitable for passing to 
 * <i>blw_start_request</i>.  Note that the wrapper/core will 
 * destroy the chan0_msg once the start request has completed, and 
 * the application should used this function to ensure correct memory 
 * management.
 * <p> BUG: NULL features, localize, server_name may not work. 
 * Use empty strings.
 *
 * @param wrap         A pointer to the wrapper structure. 
 *
 * @param channel_number The channel number.
 *
 * @param message_number The message number.
 *
 * @param op_ic Indication or Confirmation, 'I' or 'C'.
 *
 * @param op_sc Start or Close, 'S' or 'C'.
 *
 * @param profile Linked list of profile structures.
 *
 * @param error Diagnostic structure.
 *
 * @param features Features to request.
 *
 * @param localize Localization to request.
 *
 * @param server_name Server name to request.
 *
 * @return Newly allocated chan0_msg.
 *
 **/
extern CHAN0_MSG *blw_chan0_msg_new(WRAPPER *wrap,  long channel_number, 
				    long message_number, char op_ic, 
				    char op_sc, struct profile * profile,   
				    DIAGNOSTIC * error, char * features,
				    char * localize, char * server_name);

/**
 * Helper constructor for a channel 0 message.  This may be used to
 * duplicate an existing message if needed.
 *
 * @param wrap         A pointer to the wrapper structure. 
 *
 * @param msg          A pointer to the chan0_msg to be duplicated.
 *
 * @return Newly allocated chan0_msg.
 *
 **/
extern CHAN0_MSG *blw_chan0_msg_clone(WRAPPER *wrap,  CHAN0_MSG * msg);

/**
 * Destructor for a channel 0 message.  This will work correctly either 
 * on chan0_msg structs generated by the constructors and for those 
 * generated in the BEEP core.
 *
 * @param wrap         A pointer to the wrapper structure. 
 *
 * @param msg  Message to be deallocated.
 *
 **/
extern void blw_chan0_msg_destroy(WRAPPER *wrap, struct chan0_msg *msg);

extern DIAGNOSTIC * blw_diagnostic_new(WRAPPER *wrap, int code,
                                       char *lang, char *fmt, ...);
/* BUG: NEEDS DOCUMENTATION. BUG: NEEDS LOCALIZATION */

extern void blw_diagnostic_destroy(WRAPPER *wrap,
                                   DIAGNOSTIC *diagnostic);

extern struct chan_start_rpy* blw_chan_start_rpy_new(WRAPPER *wrap,
                                                     void* data, long data_len,
                                                     DIAGNOSTIC* error);

extern void waitfor_chan_stat_quiescent(WRAPPER *wrap, long channel_number);

extern void shutdown_callback(WRAPPER* wrap);

extern int blw_channel_status(WRAPPER *wrap, long channel_number);

extern int blw_thread_notify(WRAPPER *wrap, CHANNEL_INSTANCE *inst, int (*myfunc)(void*), 
                             void *ClientData);

extern DIAGNOSTIC * blw_wkr_cr_instance(WRAPPER *wrap, CHANNEL_INSTANCE * inst,
                                        struct chan0_msg *request,
                                        close_request_callback app_cr_callback,
                                        char local);

extern CHANNEL_INSTANCE *channel_instance_new(WRAPPER *wrap,
                                              PROFILE_REGISTRATION *reg, 
                                              struct chan0_msg *icz, 
                                              struct chan0_msg *ocz, 
                                              start_request_callback app_start, 
                                              close_request_callback app_close,
                                              void *ClientData);

extern void channel_instance_destroy(CHANNEL_INSTANCE *inst);

extern void blw_chan0_reply(WRAPPER *wrap, struct chan0_msg *c0m, struct profile *pro, 
                            DIAGNOSTIC *diag);
#endif
