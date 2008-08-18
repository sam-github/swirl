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

#ifndef WIN32
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#endif
#include "../utility/semutex.h"
#include "bp_fiostate.h"

#ifndef	FALSE
#define FALSE (0)
#endif
#ifndef	TRUE
#define TRUE (!FALSE)
#endif

#ifndef WIN32
#ifdef	UNPROVEN
/* the unproven package doesn't implement poll()... */

#include <string.h>
#include <sys/stat.h>
#include <sys/socket.h>

extern void log_line (int, int, char *, ...);

static char *
log_fd (int fd) {
    int             len;
    static char     buffer[BUFSIZ];
    struct stat     st;
    struct sockaddr sa;

    if (fstat (fd, &st) < 0) {
        sprintf (buffer, "fstat failed: %s", strerror (errno));
	return buffer;
    }

    if ((st.st_mode & S_IFMT) != S_IFSOCK) {
        sprintf (buffer, "mode: %07o", st.st_mode & S_IFMT);
	return buffer;
    }

    memset (&sa, 0, len = sizeof sa);
    if (getsockname (fd, &sa, &len) < 0) {
        sprintf (buffer, "getsockname failed: %s", strerror (errno));
	return buffer;
    }
    sprintf (buffer, "%d.%d.%d.%d:%d", sa.sa_data[2] & 0xff,
	     sa.sa_data[3] & 0xff, sa.sa_data[4] & 0xff, sa.sa_data[5] & 0xff,
	     ((sa.sa_data[0] << 8) & 0xff00) | (sa.sa_data[1] & 0xff));

    memset (&sa, 0, len = sizeof sa);
    if (getpeername (fd, &sa, &len) >= 0)
        sprintf (buffer + strlen (buffer), " -> %d.%d.%d.%d:%d",
		 sa.sa_data[2] & 0xff, sa.sa_data[3] & 0xff,
		 sa.sa_data[4] & 0xff, sa.sa_data[5] & 0xff,
		 ((sa.sa_data[0] << 8) & 0xff00) | (sa.sa_data[1] & 0xff));

    return buffer;
}

int
poll (struct pollfd *pfds, nfds_t count, int msecs) {
    int		nfds;
    fd_set	rfds,
		wfds,
		xfds;
    struct pollfd *pf;
    struct timeval tv;

    FD_ZERO (&rfds);
    FD_ZERO (&wfds);
    FD_ZERO (&xfds);

    nfds = -1;
    for (pf = pfds + count; --pf >= pfds; ) {
	if ((pf -> fd >= 0) && (pf -> events & (POLLIN | POLLOUT | POLLPRI))) {
	    if (pf -> events & POLLIN)  FD_SET (pf -> fd, &rfds);
	    if (pf -> events & POLLOUT) FD_SET (pf -> fd, &wfds);
	    if (pf -> events & POLLPRI) FD_SET (pf -> fd, &xfds);

	    if (pf -> fd > nfds)
	        nfds = pf -> fd;

	    log_line (0, 0, "%d: fd=%d events=0x%04x %s", pf - pfds,
		      pf -> fd, pf -> events, log_fd (pf -> fd));
	}
	pf -> revents = 0;
    }

    tv.tv_sec = msecs / 1000 , tv.tv_usec = (msecs % 1000) * 1000;

    while (1) {
        if (nfds < 0) {
	    usleep (msecs * 1000);
	    return 0;
	}

	{
	    int	  fd;
	    char *bp,
		  buffer[BUFSIZ];

	    bp = buffer;
	    for (fd = 0; fd <= nfds; fd++)
	        if (FD_ISSET (fd, &rfds)
		        || FD_ISSET (fd, &wfds)
		        || FD_ISSET (fd, &xfds)) {
		char rwx[sizeof "RWX"];

		memset (rwx, '\0', sizeof rwx);
		rwx[0] = FD_ISSET (fd, &rfds) ? 'R': '-';
		rwx[1] = FD_ISSET (fd, &wfds) ? 'W': '-';
		rwx[2] = FD_ISSET (fd, &xfds) ? 'X': '-';

		sprintf (bp, " %d:%s", fd, rwx);
		bp += strlen (bp);
	    }
	    
	    log_line (0, 0, "select nfds=%d/%d.%06d%s", nfds, tv.tv_sec,
		      tv.tv_usec, buffer);
	}
	
	if (((nfds = select (nfds + 1, &rfds, &wfds, &xfds, &tv)) >= 0)
	        || (errno != EBADF))
	    break;

        FD_ZERO (&rfds);
	FD_ZERO (&wfds);
        FD_ZERO (&xfds);

        nfds = -1;
	for (pf = pfds + count; --pf >= pfds; )
            if ((pf -> fd >= 0)
		    && (pf -> events & (POLLIN | POLLOUT | POLLPRI))) {
                fd_set rfd,
                       wfd, 
                       xfd;
                struct timeval zv;

                FD_ZERO (&rfd);
                FD_ZERO (&wfd);
                FD_ZERO (&xfd);

                if (pf -> events & POLLIN)  FD_SET (pf -> fd, &rfd);
                if (pf -> events & POLLOUT) FD_SET (pf -> fd, &wfd);
                if (pf -> events & POLLPRI) FD_SET (pf -> fd, &xfd);

		zv.tv_sec = zv.tv_usec = 0;

                if (select (pf -> fd + 1, &rfd, &wfd, &xfd, &zv) >= 0) {
                    if (pf -> events & POLLIN)  FD_SET (pf -> fd, &rfds);
                    if (pf -> events & POLLOUT) FD_SET (pf -> fd, &wfds);
                    if (pf -> events & POLLPRI) FD_SET (pf -> fd, &xfds);

                    if (pf -> fd > nfds)
                        nfds = pf -> fd;
                } else if (errno == EBADF)
                    pf -> revents |= POLLNVAL;
            }
    }
    
    if (nfds > 0)
        for (pf = pfds + count; --pf >= pfds; )
	    if (pf -> fd >= 0) {
	        if (FD_ISSET (pf -> fd, &rfds)) pf -> revents |= POLLIN;
		if (FD_ISSET (pf -> fd, &wfds)) pf -> revents |= POLLOUT;
		if (FD_ISSET (pf -> fd, &xfds)) pf -> revents |= POLLPRI;
	    }

    for (pf = pfds + count; --pf >= pfds; )
        if ((pf -> fd >= 0) && (pf -> revents != 0))
	    log_line (0, 0, "%d: fd=%d revents=0x%04x %s", pf - pfds,
		      pf -> fd, pf -> revents, log_fd (pf -> fd));

    return nfds;
}
#endif
#endif

