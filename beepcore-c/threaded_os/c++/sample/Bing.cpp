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
// Bing.cpp: implementation of the Bing class.
//
//////////////////////////////////////////////////////////////////////

#include "Bing.h"
#include "BEEPException.h"
#include "Common.h"
#include "EchoProfile.h"
#include "Peer.h"
#include "ProfileRegistry.h"
#include "Thread.h"
#include "../../../profiles/tls-profile.h"
#include "../../utility/bp_config.h"
#include "../utility/stacktrace.h"

#ifndef WIN32
#include <signal.h>
#include <unistd.h>
#define STRCASECMP strcasecmp
#else
#define STRCASECMP strcmpi
#endif

//////////////////////////////////////////////////////////////////////
// Bing is just basically putting the blocks together
//////////////////////////////////////////////////////////////////////

char usage[] = "usage: bing [-port port] [-count count] [-size size]\n"
               "            [-privacy required|preferred|none] host\n\n"
               "options:\n"
               "    -port port    Specifies the port number.\n"
               "    -count count  Number of echo requests to send.\n"
               "    -size size    Request size.\n"
               "    -privacy      required = require TLS.\n"
               "                  preferred = request TLS.\n"
               "                  none = don't request TLS.\n";

void dump_stack(int)
{
    bp_print_stack("segv: ");
    printf("pid = %d\n", getpid());
    getchar();
    abort();
}

int main(int argc, char *argv[])
{
  signal(SIGSEGV, dump_stack);
  Bing *b = new Bing();
  int result = b->run(argc, argv);
  delete(b);
  return result;
}

int Bing::parseArgs(int argc, char *argv[]) {

  if (argc < 2) {
    printf(usage);
    return -1;
  }

  int i = 0;

  while (i < (argc - 1)) {
    if (STRCASECMP(argv[i], "-port") == 0) {
      port = argv[++i];
    } else if (STRCASECMP(argv[i], "-count") == 0) {
      count = atoi(argv[++i]);
    } else if (STRCASECMP(argv[i], "-size") == 0) {
      size = atoi(argv[++i]);
    } else if (STRCASECMP(argv[i], "-privacy") == 0) {
      ++i;
      if (STRCASECMP(argv[i], "none") == 0) {
        privacy = PRIVACY_NONE;
      } else if (STRCASECMP(argv[i], "preferred") == 0) {
        privacy = PRIVACY_PREFERRED;
      } else if (STRCASECMP(argv[i], "required") == 0) {
        privacy = PRIVACY_REQUIRED;
      } else {
        printf(usage);
        return -1;
      }
    }
    ++i;
  }

  if (i != argc - 1)
    return -1;

  host = argv[argc -1];

  return 0;
}

char* Bing::initSendData()
{
	char content_type[] = "Content-Type: application/beep+xml\r\n\r\n";
	char* send_data = new char[size];
	char* temp = new char[size + strlen(content_type)];

	for (int i=0; i < size; i++ ) {
		send_data[i] = (char)((int)'a' + (i % 26));
	}		

	strcpy(temp, content_type);
	strcat(temp, send_data);
	delete(send_data);
        return temp;
}

int teststate;
CHANNEL_INSTANCE * catch_pi;

void app_start_cb(CHANNEL_INSTANCE * pi, struct chan0_msg * ocz, 
		  struct chan0_msg * icz, char local, void *ClientData)
{    
    printf("app_start_cb -- called\n");
    if (NULL == icz->error) {
	catch_pi = pi;
	teststate = 1;
	printf("app_start_cb -- OK return.\n");
    } else {
	teststate = 0;
	printf("app_start_cb -- Error return: %d -- %s\n", icz->error->code, 
	       icz->error->message);
    }

    return;
}

int Bing::run(int argc, char *argv[]) throw (BEEPException)
{
  if (parseArgs(argc, argv) != 0)
	  return -1;

  // Create our Peer and our EchoProfile
  Peer *peer = new Peer();
  EchoProfile *ep = new EchoProfile();

  // Create a ProfileRegistry and populate it with a
  // ProfileRegistration from our EchoProfile
  ProfileRegistry *reg = new ProfileRegistry();
  ProfileRegistration *pr = ep->createProfileRegistration();
  reg->addProfileRegistration(pr);

  if (privacy != PRIVACY_NONE) {
    struct configobj *beep_config;
    if (!(beep_config = config_new(NULL))) {
      printf("Fatal:  Init config failed.\n");
      return -1;
    }

    config_set(beep_config, "beep profiles tls fullmessages", "1");
    config_set(beep_config, "beep profiles tls threadperchannel", "0");
    config_set(beep_config, "beep profiles tls uri",
               "http://iana.org/beep/TLS");
    config_set(beep_config, "beep profiles tls certfile", "testone.pem");
    config_set(beep_config, "beep profiles tls keyfile", "testone.pem");

    catch_pi = NULL;
    teststate = 0;

    DIAGNOSTIC * diag;
    CHANNEL_INSTANCE * my_pi_1 = NULL;
    PROFILE_REGISTRATION * tlsp = tls_profile_Init(beep_config);
    if (tlsp == NULL) {
      printf("Error initializing TLS.\n");
      return -1;
    }
    pr = new ProfileRegistration(tlsp);
    reg->addProfileRegistration(pr);
  }

  // Get a Session
  Session *session = peer->initiateTCPSession(host, port, reg);    

  if (privacy != PRIVACY_NONE) {
    CHANNEL_INSTANCE * my_pi_1 = NULL;
    DIAGNOSTIC *diag;
    diag = tls_tuning_reset_request(session->getWrapper(), 7, app_start_cb, 
                                    NULL, 1);
    if (diag != NULL) {
      printf("error %s\n",diag->message);
      blw_diagnostic_destroy(session->getWrapper(),diag);
      return -1;
    }

    while (teststate == (-1) &&
           session->getWrapper()->status != INST_STATUS_EXIT)
    {
      sleep(1);
    }

    my_pi_1 = catch_pi;
    if (my_pi_1 == NULL) {
      printf("unable to start TLS profile.\n");
      return -1;
    }

#if 0
    if (session->getWrapper()->status != INST_STATUS_EXIT) {
      tls_app_send_ready(my_pi_1->profile);
    }
#endif

    diag = blw_wait_for_greeting(session->getWrapper());

    if (session->getWrapper()->status == INST_STATUS_EXIT) {
      printf("Session closed\n");
      return -1;
    }
  }

  Channel *ch = session->startChannel(ep->getUri());

  // Send a message on the channel
  char* data = initSendData();		


  for (int i=0; i < count; i++ ) {
    Message *request = new Message(data, strlen(data), 
                                   ch, Frame::MESSAGE_TYPE, -1);
    Message *reply = ch->sendMessage(request);
    // Free up the request now that we have a reply
    delete(request);

    // Get the reply...
    //    unsigned int reply_size = reply->getSize();
    char *buffer = new char[size];
    //    for (int count=0;;) {
      int n = reply->read(buffer, count, size - count);
    //      if (n < 0)
    //        break;
    //      count += n;
    //    }
      //    buffer[size]=0; // Necessary?
    delete[] buffer;

    printf("Reply from %s: bytes=%d time=%dms\n",
           host, reply->getSize(), 0);
    // Now that we're done with it
    delete(reply);
  }

  // free data
  delete[] (data);

  // Be happy, close the channel
  try {
    session->closeChannel(ch);
  } catch (BEEPException) {
    printf("Caught close exception\n");
  }

  // Cleanup
  delete(ch);

  // Close the Session
  session->close();
  //  Thread::sleep(1000);

  delete(session);
  
  // Shut the Peer down
  peer->shutdown();
  delete(peer);

  return 0;
}
