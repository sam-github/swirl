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
// EchoServer.cpp: implementation of the EchoServer class.
//
//////////////////////////////////////////////////////////////////////

#include "EchoProfile.h"
#include "EchoServer.h"
#include "Thread.h"

//////////////////////////////////////////////////////////////////////
// Main Routine
//////////////////////////////////////////////////////////////////////

int main(int argc,char *argv[])
{
  if (argc != 3)
    {
      printf("usage: %s server port\n", argv[0]);
      return 1;
    }

  char **args = new char *[4];
  args[0] = argv[0];
  args[1] = argv[1];
  args[2] = argv[2];
  args[3] = "L";

  printf("Echo Server ", NULL);
  dprintf("using Host=>%s", argv[1]);
  dprintf(" and port=>%d\n", atoi(argv[2]));

  // Create our Peer and our EchoProfile
  printf("Creating Peer\n");
  Peer *peer = new Peer();
  printf("Creating Echo Profile\n");
  EchoProfile *ep = new EchoProfile();

  // Create a ProfileRegistry and populate it with a
  // ProfileRegistration from our EchoProfile
  printf("Creating and populating ProfileRegistry\n");
  ProfileRegistry *reg = new ProfileRegistry();
  ProfileRegistration *pr = ep->createProfileRegistration();
  reg->addProfileRegistration(pr);
  
  // Get a Session
  printf("Listening for a session\n");
  Session *s = (Session*)1;
  // TODO alternate loop code to test cleanup
  //for(int k=0; k<4; k++)
  while(s != NULL)
  {
    Session *s = peer->listenForTCPSession(argv[1], argv[2], reg);
    printf("EchoServer has accepted a connection and created a session %x\n", s);
    printf("Waiting for more connections...\n", NULL);
  }
  // Part of spoofed alternate loop code to test cleanup
  //Thread::sleep(20000);
  delete(reg);
  delete(ep);
  peer->shutdown(0);
  delete(peer);
  delete(args);
  dprintf("EchoServer EXIT\n", NULL);
  exit(0);
}

