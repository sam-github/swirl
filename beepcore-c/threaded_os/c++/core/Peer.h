/*
 * Copyright (c) 2001 Invisible Worlds, Inc.  All rights reserved.
 *
 * The contents of this file are subject to the Blocks Public License (the
 * "License"); You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at http://www.invisible.net/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.  See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 */
// Peer.h: interface for the Peer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PEER_H__BB039439_479A_4116_8435_70DABB37CBE7__INCLUDED_)
#define AFX_PEER_H__BB039439_479A_4116_8435_70DABB37CBE7__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Common.h"
#include "Message.h"
#include "ProfileRegistry.h"
#include "Session.h"
#include "SessionContext.h"
#include "Thread.h"
#include "bp_wrapper.h"


#ifdef WIN32
#pragma warning(disable:4786)
#include <windows.h>
#endif

// Used to start and destroy the session and the associated
// profile registraiton chains
struct sessionStartupStruct
{
  int argc;
  char **args;
  int socket;
  PROFILE_REGISTRATION *registration;
  WRAPPER *(*callback)(int, char, IO_STATE *, PROFILE_REGISTRATION*);
};

#include <list>
using namespace std;
typedef list<Session*> SessionList;

// Right now I'm using the Peer as more of an aggregation point to develop
// user-level helper functionality later.  The SessionContext is what
// encapsulates the wrapper instaed.
class Peer  
{
public:
	Peer();
	virtual ~Peer();
	
	void addSession(Session *s);
	void printSessionList();
	void removeSession(Session *s);
	void terminate();

	ProfileRegistry *getDefaultProfileRegistry()
	  {return defaultRegistry;}

	// These calls use the default PEER ProfileRegistry (from 
	// configuration) if it's available.
	Session *initiateTCPSession(char *host, char *port,
				    ProfileRegistry *pr = NULL);
	Session *initiateTCPSession(int socket,
				    ProfileRegistry *pr = NULL);
	Session *listenForTCPSession(char *host, char *port,
				     ProfileRegistry *pr = NULL);
	Session *listenForTCPSession(int socket,
				     ProfileRegistry *pr = NULL);
	char *createSessionIdentifier(char *host, char *port, char mode);

	void shutdown();
	void shutdown(DIAGNOSTIC *diag);
	static void terminate(Peer *p, DIAGNOSTIC *diag);

	// Used to workaround the forking listener issue,
	// since I can't get anywhere with the threaded library.
	static bool startupFlag;
	static Mutex startupLock;
	static Condition startupCondition;
	static WRAPPER *startupWrapper;

private:
	// Used for general global control
	static bool acceptingConnections, initialized, listeningOnAnySessions;
	static unsigned int sessionCounter;

	// Constants
	static unsigned int channelStartOdd, channelStartEven;
	static char COMMA[], INITIATOR_MODE, LISTENER_MODE, COLON;
	static char INITIATOR_MODE_STR[], LISTENER_MODE_STR[];

	// Data Members
	Mutex lock;
	ProfileRegistry *defaultRegistry;
	SessionList sessions;
	Thread *listeningThread;
	
	//static SIF *startupHooks;
	static char *peer_session_init(PROFILE_REGISTRATION *reg,
				       WRAPPER *wrap);
	static void sessionNotify(void *d);
};

#endif // !defined(AFX_PEER_H__BB039439_479A_4116_8435_70DABB37CBE7__INCLUDED_)
