/*
 * Copyright (c) Huston Franklin  All rights reserved.
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
#ifndef BP_TCP_H
#define BP_TCP_H

#include "../wrapper/bp_wrapper.h"

/**
 * @name Convenience functions for TCP-based connections
 */
//@{

#define INITIATOR 'I'
#define LISTENER 'L'

/**
 * Initializes the TCP connection library, invoking
 * {@link bp_library_init bp_library_init}.
 * <p>
 * This must be the first library function called.
 **/ 
extern void tcp_bp_library_init();

/**
 * Terminates the TCP connection library.
 * <p>
 * This function does not call {@link bp_library_cleanup bp_library_cleanup}.
 **/ 
extern void tcp_bp_library_shutdown();

/**
 * Front-end to {@link bp_connection_create bp_connection_create}. This
 * creates a {@link BP_CONNECTION BP_CONNECTION} from an already connected
 * tcp socket.
 * 
 * Consider using {@link tcp_bp_connect tcp_bp_connect} and
 * {@link tcp_bp_listen tcp_bp_listen} instead.
 */
extern BP_CONNECTION*
tcp_bp_connection_create(int socket,
                         char *mode,
                         void (*log)(int,int,char *,...),
                         struct configobj *appconfig);

/**
 * Connect to a host, bind the socket-descriptor to a connection structure,
 * register profile-modules, and start the initiator.
 *
 * @param host A pointer to the hostname or dotted-quad.
 *
 * @param port A TCP port number.
 *
 * @param log A pointer to the logging function, which is usually
 *            {@link log_line log_line}.
 *
 * @param pr A pointer to the profile-registration structure.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure that may be used for configuration purposes.
 *
 * @param connection A pointer to return a new pointer to a
 *                   {@link BP_CONNECTION connection} structure.
 *
 * @return A diagnostic structure if an error occured while connecting,
 *	   otherwise <b>NULL</b>.
 *
 * @see bp_connection_create
 * @see bp_profile_register
 * @see bp_start
 */
extern DIAGNOSTIC* tcp_bp_connect(char* peer,
                                  int port,
                                  PROFILE_REGISTRATION* pr,
                                  struct configobj* appconfig,
                                  BP_CONNECTION** connection);

/**
 * Listen for incoming connections, bind each socket-descriptor to a
 * connection structure, register profile-modules, and start the listener.
 *
 * @param host A pointer to the hostname or dotted-quad.
 *
 * @param port A TCP port number.
 *
 * @param pr A pointer to the profile-registration structure.
 *
 * @param appconfig A pointer to the {@link configobj configuration}
 *                  structure that may be used for configuration purposes.
 *
 * @return A diagnostic structure if an error occured while opening the
 *	   socket, otherwise (eventuall) <b>NULL</b> if the listener is
 *         terminating gracefully.
 *
 * @see bp_connection_create
 * @see bp_profile_register
 * @see bp_start
 */
extern DIAGNOSTIC* tcp_bp_listen(char* host,
                                 int port,
                                 PROFILE_REGISTRATION* pr,
                                 struct configobj* appconfig);

/**
 * Closes the socket associated with the connection returned by
 * {@link tcp_bp_connect tcp_bp_connect} and calls
 * {@link bp_connection_destroy bp_connection_destroy}.
 *
 * @param conn A pointer to the connection structure. 
 */
extern void tcp_bp_connection_close(BP_CONNECTION* conn);

//@}

#endif
