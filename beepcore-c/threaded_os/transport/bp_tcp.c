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
#ifndef WIN32
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#else
#include <windows.h>
#endif

#include "../wrapper/bp_wrapper.h"
#include "../utility/bp_config.h"
#include "../utility/logutil.h"
#include "bp_fiostate.h"
#include "bp_tcp.h"

#define MAX_INT_STRING_LENGTH  10

int TERMINATE = 0;

#ifdef WIN32
#define CLOSE_SOCKET(_socket) closesocket(_socket)
#else
#define CLOSE_SOCKET(_socket) close(_socket)
#endif

/*
 * This file the start of an attempt to consolidate all of the beeptcp_* APIs
 * and present a consistent interface for the tcp mapping layer. It is by
 * no means complete or perfect... just better.
 */
void tcp_bp_library_init()
{
    int max_poll;
#ifdef WIN32
    WSADATA wsadata;
  
    WSAStartup(MAKEWORD(2,2), &wsadata);
#endif

    bp_library_init(malloc, free);

    max_poll = IW_fpoll_max(256);
    fpollmgr_init(max_poll);
}

void tcp_bp_library_shutdown()
{
    fpollmgr_shutdown();
}

BP_CONNECTION* tcp_bp_connection_create(int socket, char* mode,
                                        void (*log)(int,int,char *,...),
                                        struct configobj* appconfig)
{
    IO_STATE* ios;

#ifdef WIN32
    /* make incoming connection non_blocking */
    u_long argp = 1;
    if (ioctlsocket(socket, FIONBIO, &argp) < 0) {
        return NULL;
    }
#else
    if (fcntl(socket,F_SETFL,O_NDELAY) < 0) {
        return NULL;
    }
#endif

    ios = fiostate_new(socket);
    if (!ios) {
        return NULL;
    }

    return bp_connection_create(mode, ios, log_line, appconfig);
}

/*

   update the sockaddr_in with the server address given the server name
   input by domain or host or dot notation.
   A return of 0 is successful.

*/ 
static int IW_fcbeeptcp_parseinet(struct sockaddr_in *soc_in, char *domainname,
                                  char mode)
{
    int dotcount_ip = 0;
    struct hostent  *phost;
    unsigned int len=0;
    char *tloc="localhost";
    char *strptr;

    if (*domainname >= '0' && *domainname <= '9')
    {
        len = 0;
        strptr = domainname;
        while (*strptr)
        {
            if (!isdigit((int)*strptr) && *strptr != '.')
                break;
            else
                len++;
            if (*strptr == '.')
                dotcount_ip++;
            strptr++;
        }
        if (*strptr)
            dotcount_ip = 0;
    }

    if (dotcount_ip == 3)
        soc_in->sin_addr.s_addr = inet_addr(domainname);
    else
    {
        if (len == strlen(domainname)) {
            if (mode == 'L' || mode == 'l') {
                soc_in->sin_addr.s_addr = INADDR_ANY;
                return 0;
            }
            phost = gethostbyname(tloc);
        }
        else
            phost = gethostbyname(domainname);
        if (!phost)
        {
            memset((void *)&soc_in->sin_addr, 0, sizeof(soc_in->sin_addr));
            return 1;
        }
        else
            memcpy((void *)&soc_in->sin_addr,
                   phost->h_addr, phost->h_length);
    }
    return 0;
}

DIAGNOSTIC* tcp_bp_connect(char* peer, int port,
                           PROFILE_REGISTRATION* reg,
                           struct configobj* appconfig,
                           BP_CONNECTION** connection)
{
    BP_CONNECTION* conn;
    int sock;
    PROFILE_REGISTRATION *myreg;
    struct sockaddr_in server;

    *connection = NULL;

    memset (&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons((u_short)port);
    if (IW_fcbeeptcp_parseinet(&server, peer, 'I')) {
        return bp_diagnostic_new(NULL, 420, NULL,
                                 "unable to resolve address: %s",
#ifndef WIN32
                                 hstrerror(h_errno));
#else
                                 peer);
#endif
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return bp_diagnostic_new(NULL, 420, NULL, "unable to open socket: %s",
                                 strerror(errno));
    }

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        CLOSE_SOCKET(sock);
        return bp_diagnostic_new(NULL, 420, NULL,
                                 "unable to establish connection: %s",
                                 strerror(errno));
    }

    conn = tcp_bp_connection_create(sock, "plaintext", log_line, appconfig);
    if (conn == NULL) {
        CLOSE_SOCKET(sock);
        return bp_diagnostic_new(NULL, 420, NULL,
				 "bp_connection_create failed");
    }

    myreg = reg;
    while (myreg) {
        DIAGNOSTIC* diag = bp_profile_register(conn, myreg);
        if (diag) {
            bp_connection_destroy(conn);
            CLOSE_SOCKET(sock);
            return diag;
        }
        myreg = myreg->next;
    }

    bp_start(conn, INITIATOR);

    *connection = conn;
    return NULL;
}

#ifndef WIN32
BP_HASH_TABLE *hash_children;
/* set up default signal handlers */
void IW_fcbeeptcp_terminate(int isignal)
{
    BP_HASH_SEARCH *hs;
    unsigned long k;


    hs = bp_hash_search_init(hash_children);
    for (k=bp_hash_search_int_firstkey(hs);
         k != NO_KEY;
         k=bp_hash_search_int_nextkey(hs))
    {
        kill(k,SIGTERM);
    }
    bp_hash_search_fin(&hs);
    printf("beep server going down on signal %d\n",isignal);
  
    TERMINATE=1;
    return;
}

void IW_fcbeeptcp_reread_config(int isignal)
{
    return;
}

void IW_fcbeeptcp_sigchild(int isignal)
{
    int istatus;
    pid_t child;
    unsigned long **tmp;

    while ((child = waitpid(-1,&istatus,WNOHANG|WUNTRACED)) > 0)
    {
        tmp = bp_hash_int_get_key(hash_children,child);
        if (tmp)
        {
            if (**tmp == child)
            {
                bp_hash_int_entry_delete(hash_children,child);
                bp_hash_free((void **)tmp);
            }
        }
    }
    return;
}

void IW_cbeeptcp_low_signalset()
{
    struct sigaction act;

    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask,SIGTERM);
    act.sa_flags = SA_NOCLDSTOP;
    act.sa_handler = IW_fcbeeptcp_terminate;
    sigaction(SIGTERM,&act,NULL);
    act.sa_handler = IW_fcbeeptcp_reread_config;
    sigaction(SIGHUP,&act,NULL);
    act.sa_handler = IW_fcbeeptcp_sigchild;
    sigaction(SIGCHLD,&act,NULL);
    return;
}
#endif

DIAGNOSTIC* tcp_bp_listen(char* host, int port, PROFILE_REGISTRATION* reg,
                          struct configobj* appconfig)
{
#ifdef WIN32
    fd_set readFds;
#else
    struct pollfd ready[1];
    int one = 1;
#endif

#ifdef	SOMAXCONN
    int    backlog = SOMAXCONN;
#else
    int    backlog = 3;
#endif
#ifndef WIN32
    int sock;
#else
    SOCKET sock;
#endif
    struct sockaddr_in server;

    memset (&server, 0, sizeof server);
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons((u_short)port);
    if (IW_fcbeeptcp_parseinet(&server, host, 'L')) {
        return bp_diagnostic_new(NULL, 420, NULL,
                                 "unable to resolve address: %s",
#ifndef WIN32
                                 hstrerror(h_errno));
#else
                                 host);
#endif
    }

    sock = socket(AF_INET,SOCK_STREAM,0);
    if (sock < 0) {
        return bp_diagnostic_new(NULL, 420, NULL, "unable to open socket: %s",
                                 strerror(errno));
    }

#ifndef WIN32
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void*)&one, sizeof(int));
#endif
    if ((bind(sock, (struct sockaddr *)&server, sizeof(server))) < 0) {
        CLOSE_SOCKET(sock);
        return bp_diagnostic_new(NULL, 420, NULL,
                                 "unable to bind socket: %s",
                                 strerror(errno));
    }

#ifndef WIN32
    IW_cbeeptcp_low_signalset();
#endif

#ifdef WIN32
    FD_ZERO(&readFds);
    FD_SET(sock, &readFds);
#else
    ready[0].fd = sock;
    ready[0].events = POLLIN|POLLPRI;
    ready[0].revents = 0;
#endif

    /**
     * @todo make configurable -
     * the maximum number of open sockets file descriptors
     */

    listen(sock,backlog);
    log_line (LOG_WRAP, 2, "listening on %d (backlog %d)", sock, backlog);

    do {
#ifdef WIN32
        if (select(1, &readFds, NULL, NULL, NULL) <= 0)
#else
        int	msecs = 5;

        if (poll(ready, 1, msecs) <= 0)
#endif
        {
#ifdef WIN32
            FD_ZERO(&readFds);
            FD_SET(sock, &readFds);
#endif
            if (TERMINATE) {
                return NULL;
            }
#ifndef WIN32
            YIELD();
#endif
            continue;
        }
#ifdef WIN32
        if (FD_ISSET(sock, &readFds))
#else
        if ((ready[0].revents & POLLIN) == POLLIN)
#endif
        {
            BP_CONNECTION* conn;
            PROFILE_REGISTRATION *myreg;
            DIAGNOSTIC *diag;
            int msgsock;

            /* accept new connection */
            msgsock = accept(sock,(struct sockaddr *)0,(int *)0);
            if (msgsock < 0)
                continue;

            conn = tcp_bp_connection_create(msgsock, "plaintext", log_line,
                                            appconfig);
            if (!conn) {
                CLOSE_SOCKET(msgsock);
                continue;
            }

            myreg = reg;
            while (myreg) {
                diag = bp_profile_register(conn, myreg);
                if (diag) {
                    bp_log (conn, LOG_WRAP, 3, "%s", diag->message);
                    bp_diagnostic_destroy(conn,diag);
                    diag=NULL;
                }
                myreg=myreg->next;
            }

            bp_start(conn, LISTENER);
        }
    } while(!TERMINATE);

    return NULL;
}

void tcp_bp_connection_close(BP_CONNECTION* conn)
{
    IO_STATE* ios = bp_get_iostate(conn);
    int socket = ios->socket;

    bp_connection_destroy(conn);

    CLOSE_SOCKET(socket);
}
