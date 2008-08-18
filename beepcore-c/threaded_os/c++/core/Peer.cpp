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
// Peer.cpp: implementation of the Peer class.
//
// This class is the analog of the Wrapper, more or less
// in the C library
// 
//////////////////////////////////////////////////////////////////////

#include "BEEPException.h"
#include "Common.h"
#include "Peer.h"
#include "PeerConfiguration.h"
#include "bp_fcbeeptcp.h"
#include "bp_wrapper.h"
#include "logutil.h"
#include "bp_malloc.h"
#include "bp_config.h"
#include "bp_slist_utility.h"
#include "cbtcphelper.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// Global tracking stuff
unsigned int Peer::sessionCounter = 0;

// Constants
char Peer::COLON = ':';
char Peer::COMMA[] = ",";
char Peer::INITIATOR_MODE = 'i';
char Peer::INITIATOR_MODE_STR[] = "i";
char Peer::LISTENER_MODE = 'l';
char Peer::LISTENER_MODE_STR[] = "l";

// Accoutrements of the hideous hack
unsigned int Peer::channelStartOdd = 1;
unsigned int Peer::channelStartEven = 2;

//static int peer_count;

// Done static so we lock and prevent weird serial clobbering of starts.
// No way to maintain object state in these initial calls without mod'ing C lib.
bool Peer::acceptingConnections = false;
bool Peer::initialized = false;
bool Peer::startupFlag = false;

Mutex Peer::startupLock = Mutex();
Condition Peer::startupCondition = Condition(NULL);
WRAPPER *Peer::startupWrapper = NULL;

WRAPPER *wrapper_callback(int msgsock,char mode,IO_STATE *ios,PROFILE_REGISTRATION *reg);
#if defined(WIN32)
void * WINAPI listenAndAcceptRoutine(void *arg);
#else
void *listenAndAcceptRoutine(void *arg);
#endif

// TODO It is envisioned that the Peer will have a 'nice' way of
// reading configuration files (via PeerConfiguration) and thereafter
// setting all sorts of things based on the config, such as modes,
// profile registrations, and the like.
Peer::Peer()
  :defaultRegistry()
{
  // Initialized the wrapper library if necessary
  // TODO worry about contention here?
  if(!initialized)
    {
      initialized = true;
      bp_library_init(malloc, free);
    }
  listeningThread = NULL;
  //peer_count++;
  //dprintf("Peer::Peer count=>%d\n", peer_count);
}

Peer::~Peer()
{
  // Smoke all our Sessions, SessionContexts etc. to 
  // start the parade of death
  //peer_count--;
  //dprintf("Peer::~Peer count=>%d\n", peer_count);
}

void Peer::addSession(Session *s)
{
  //sessions.insert(SessionTable::value_type((string)s->getIdentifier(), s));
  sessions.push_back(s);
  dprintf("Peer added session %s, \n", s->getIdentifier());
  dprintf("session table size %d\n", sessions.size());
}

void Peer::removeSession(Session *s)
{
  //sessions.erase(string)s->getIdentifier());
  sessions.remove(s);
  dprintf("Peer removed session %s, \n", s->getIdentifier());
  dprintf("session table size %d\n", sessions.size());
  // TODO
  //delete(s);
}

void Peer::terminate()
{
  //Thread::killAllThreads();
}

Session *Peer::initiateTCPSession(char *host, char *port,
				  ProfileRegistry *pr)
{
  // Leverage what they did in wrapper land
  dprintf("Creating TCPSession as Initiator on %s\n", host);

  if(pr == NULL)
    pr = defaultRegistry;

  WRAPPER *wrapper = NULL;
  char **args = new char *[4];

  args[0] = NULL; // Whatever
  args[1] = host;
  args[2] = port;
  args[3] = Peer::INITIATOR_MODE_STR;
  PROFILE_REGISTRATION *p = pr->getProfileList();

#ifdef WIN32
  lib_malloc_init(malloc, free);
  bp_library_init(malloc, free);
  int mysock = IW_cbeeptcp_low_connect(4, args);
  if (mysock == (-1))
  {
      perror("connection failed\n");
      exit (1);
  }
  
  IW_cbeeptcp_setup_polling(1);
  
  IO_STATE* iostate = IW_cbeeptcp_monitor_fd(mysock);
  if (!iostate) 
  {
      perror("failed to add socket to poller \n");
      exit (1);
  }

  char* log_file = (char *)lib_malloc(16);
  sprintf(log_file, "IW_log%3.3d.log", mysock);
  log_create(LOG_MODE_FILE, log_file, "Peer", 0, LOG_USER);
  lib_free(log_file);

  wrapper = blw_wrapper_create(mysock, "plaintext", 'I', iostate, log_line, NULL,
      NULL);

  if(wrapper == NULL)
    {
      // TODO Throw Exception
      dprintf("Unable to connect to host=>%s ", args[1]);
      dprintf("port=>%s\nExiting Program\n", args[2]);
      throw BEEPException();
    }

  while (p) {
      DIAGNOSTIC* diag = blw_profile_register(wrapper, p);
      if (diag) {
          wrapper->log(LOG_WRAP, 0, diag->message);
          blw_diagnostic_destroy(wrapper, diag);
          diag = NULL;
      }
      p = p->next;
  }

  blw_start(wrapper,'I');
#else
  wrapper = IW_fcbeeptcp_peer(4, args, p, 0, 0);
  if(wrapper == NULL)
    {
      // TODO Throw Exception
      dprintf("Unable to connect to host=>%s ", args[1]);
      dprintf("port=>%s\nExiting Program\n", args[2]);
      throw BEEPException();
    }
#endif

  dprintf("Waiting for Greeting...\n", NULL);
  DIAGNOSTIC *diag = blw_wait_for_greeting(wrapper);
  if(diag != NULL)
    {
      // TODO Throw Exception
      dprintf("Unable to get greeting from peer at host=>%s ", args[1]);
      dprintf("port=>%s\nExiting Program\n", args[2]);
      throw BEEPException();
    }
  
  //dprintf("Wrapper is %x \n", wrapper);
  //dprintf("Session is %x\n", wrapper->session);
  Session *s = new Session(new SessionContext(wrapper), this, p, channelStartOdd,
			   createSessionIdentifier(host,port,INITIATOR_MODE));
  wrapper->externalSessionReference = (void*)s;
  addSession(s);
  delete(args);
  printSessionList();
  return s;
}

// TODO TBD
Session *Peer::initiateTCPSession(int socket, ProfileRegistry *reg )
{
  return NULL;
}

// TODO TBD
Session *Peer::listenForTCPSession(int socket, ProfileRegistry *reg)
{
  return NULL;
}

void Peer::printSessionList()
{
  int i = sessions.size();
  if(i==0)
    return;

  dprintf("\nSESSION LIST <SIZE = %d>\n", i);
  i = 0;
  for(SessionList::iterator it = sessions.begin(); it != sessions.end(); it++)
    {
      Session *temp = (Session*)(*it);
      dprintf("%d -> ", i++);
      dprintf("%s\n", temp->getIdentifier());
    }
  dprintf("\nEND SESSION LIST \n", NULL);
}

// TODO needs to be more involved so that it could support multiple
// sessions with the same Peer...right now I'm appending a # that 
// I increment for each session...
char *Peer::createSessionIdentifier(char *host, char *port, char mode)
{
  lock.lock();
  char *sessionNumber = new char[32];
  sprintf(sessionNumber, "#%i", (sessionCounter++));
  lock.unlock();
  unsigned int length = strlen(host) + strlen(port) + 3 + strlen(sessionNumber), i = 0;
  char *result = new char[length];

  // Do the host:
  length = strlen(host);
  strncpy(result, host, length);
  result[length] = COLON;

  // Do the port part
  i = length+1;
  length = strlen(port);
  strncpy(&result[i], port, length);
  i += length;
  result[i] = mode;

  // Do the unique number bit
  length = strlen(sessionNumber);
  strncpy(&result[i+1], sessionNumber, length);
  result[i+1+length]=0;

  //dprintf("Generated ID=>%s\n", result);
  delete[] sessionNumber;
  return result;
}

void Peer::shutdown()
{
  shutdown(NULL);
}

void Peer::shutdown(DIAGNOSTIC *diag)
{
  // Close all of our sessions/wrappers/channels by shutting the sessions down
  // or at least attempting to
  
  dprintf("Peer Exiting Normally\n", NULL);
  if(sessions.size() > 0)
    {
      dprintf("Peer is enumerating and closing its sessions %d\n", sessions.size());
      for(SessionList::iterator it = sessions.begin(); it != sessions.end(); it++)
	{
	  Session *temp = (Session*)(*it);
	  temp->close();
	  //delete(temp);
	}
    }
  else
    dprintf("Peer is skipping session cleanup (nothing to do)", NULL);

  this->terminate(this, diag);
  if(listeningThread != NULL)
    Thread::join(listeningThread);
}

void Peer::terminate(Peer *p, DIAGNOSTIC *diag)
{
  // Do other ancillary cleaning up
  dprintf("Peer::terminate\n", NULL);
  if(diag != NULL)
    {
      int code = diag->code;
      dprintf("Peer Terminating\nREASON=>%s\n", diag->message);
      //Use the real delete for this TODO
      //blw_diagnostic_destroy(???,diag);
      blw_diagnostic_destroy(NULL,diag);
    }

  //delete(p);
  //dprintf("Peer deleted\n", NULL);
  IW_fpollmgr_shutdown();

  // Tell the wrapper library to cleanup
  if(initialized)
    {
      bp_library_cleanup();
      initialized = false;
    }
}

void Peer::sessionNotify(void *d)
{
  dprintf("SessionNotifier was called!\n", NULL);
  Condition *c = (Condition*)d;
  c->notify();
}

Session *Peer::listenForTCPSession(char *host, char *port, 
				   ProfileRegistry *pr)
{
  char **args = new char*[4];
  int sock, max_poll;
  WRAPPER *wrapper;
  args[0] = host; // Whatever
  args[1] = host;
  args[2] = port;
  args[3] = Peer::LISTENER_MODE_STR;
  PROFILE_REGISTRATION *preg = pr->getProfileList();

  // TODO key the host/port pair to some map that we use to
  // store listeners - so we can have accepting threads going on
  // a variety of different interfaces.  Right now, it's locked
  // into basically the first one you use - 

  // For now - listen/bind once and setup callback to come
  // through and notify us below this block (where we wait)
  // in the event that a passive (listening) session has begun
  startupLock.lock();
  if(!acceptingConnections)
    {
      acceptingConnections = true;
      dprintf("Binding, listening, and accepting...first time through\n", NULL);

      sock = IW_cbeeptcp_low_connect(4, args);
      if (sock == (-1))
	{
	  // TODO throw exception

	  // Reset our flag (we're not listening)
	  // and release the lock
	  acceptingConnections = false;
	  startupLock.unlock();
	  return NULL;
	  //perror("connection failed\n");
	  //exit (1);
	}
      
      /* this step is only required for a polling threaded server */
      max_poll = IW_fpoll_max(256);
      /* set up polling thread */   
      IW_cbeeptcp_setup_polling(max_poll);
     
      /* Thread this off and then free ourselves up to return */
      // First store the data we need to pass in to maintain context
      // SSS Allocation
      struct sessionStartupStruct *sss = new sessionStartupStruct();
      sss->argc = 4;
      sss->args = args;
      sss->socket = sock;
      sss->callback = wrapper_callback;
      sss->registration = preg;

      dprintf("Peer::listenForTCP LOCKING\n", NULL);

      // Spin thread off 
      startupFlag = false;
      dprintf("Peer::listenForTCP - threading off listening thread\n", NULL);

      // TODO check return value
      listeningThread = Thread::startThread(listenAndAcceptRoutine, sss);

      // Set our flags accordingly
      // TODO use another global bool to track whether we're in here so the
      // logic of acceptingConnections is accurate
      // I'm forced to set the flog to block others from entereing this loop
      // before I unlock - so we need another flag.
      while(startupFlag != true)
      {
	startupLock.unlock();
	Thread::yield();
	startupLock.lock();
      }

      // We're locked here
      // Serialize requests going through this path so we can
      // track wrapper->object mapping as we jump across threads etc.
      dprintf("Peer::listenForTCP WAITING\n", NULL);
    }

  //Block until we hear back from the wrapper
  if(startupWrapper == NULL)
    startupCondition.wait(&startupLock);

  // Assign the last one that got accepted...
  // and clear it so it doesn't get used again.
  dprintf("Peer::listenForTCP assigning and clearing startupWrapper", NULL);
  wrapper = startupWrapper;
  startupWrapper = NULL;

  //dprintf("Peer::listenForTCP UNLOCKING\n", NULL);
  startupLock.unlock();
  //dprintf("Peer::lft session %x\n", wrapper->session);

  // Register the profiles they've passed in.
  dprintf("profileList %x\n", preg);

  // Create a session from it
  Session *result = new Session(new SessionContext(wrapper), this, preg, 
				channelStartEven,
				createSessionIdentifier(host,port,
							INITIATOR_MODE));
  wrapper->externalSessionReference = result;

  // Store the session
  sessions.push_back(result);
  printSessionList();

  // Send the Session back
  return result;
}

#if defined(WIN32)
void * WINAPI listenAndAcceptRoutine(void *arg)
#else
void *listenAndAcceptRoutine(void *arg)
#endif
{
  dprintf("Listener and accept routine has been spawned\n",NULL);
  
  // Unpack the argument and block reading requests
  struct sessionStartupStruct *sss = (struct sessionStartupStruct *)arg;

  dprintf("Logging with %x", sss);
  dprintf(" %d\n",sss->socket);
  char log_file[16];
  sprintf(log_file,"bp_log%3.3d.log",sss->socket);
  log_create(LOG_MODE_FILE, log_file, "lowechoserv", 0, 0);
  
   // This only gets changed once, so we won't lock.
  // It's kind of reliant on the locking around the listeningAndAccepting stuff
  Peer::startupFlag = true;
  IW_cbeeptcp_low_listen(sss->argc, sss->args, sss->socket, 
			 sss->registration, sss->callback);
  
  // Cleanup before exit
  log_destroy();
  blw_profile_registration_chain_destroy(NULL,sss->registration);
  // SSS Freed
  delete(sss->args);
  delete(sss);
  dprintf("Listener and accept routine exiting\n",NULL);
  
  return NULL;
}

WRAPPER *wrapper_callback(int msgsock,char mode,IO_STATE *ios,PROFILE_REGISTRATION *reg)   
{
  WRAPPER *wrap;
  
  dprintf("Peer::wrapper_callback\n", NULL);
  Peer::startupLock.lock();

 /* create the wrapper -- 
     use the function blw_set_reader_writer if your application needs to 
     control the io or bind the socket to a third party socket 
     application i.e. Openssl */
  //dprintf("Creating wrapper\n", NULL);
  wrap = blw_wrapper_create(msgsock,"plaintext",mode,ios,
			    log_line, NULL, NULL);
  
  // Given what we've done above, we'll only get called while this is locked
  dprintf("Peer::wrapper_callback setting wrapper\n", NULL);
  blw_start(wrap,mode);
  dprintf("Assigning wrapper %x\n", wrap);
  Peer::startupWrapper = wrap;
  Peer::startupCondition.notify();    
  Peer::startupLock.unlock();
  return wrap;
}
