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
// ProfileRegistration.cpp: implementation of the ProfileRegistration class.
//
//////////////////////////////////////////////////////////////////////

#include "Profile.h"
#include "ProfileRegistration.h"

// Global Static Stuff
const char ProfileRegistration::PLAINTEXT[] = "plaintext";

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// TODO simplify, probably by removing the last two bool parms for now.
ProfileRegistration::ProfileRegistration(char *uri,
                                         Profile *profile,
                                         char *listenerModes,
                                         char *initiatorModes,
                                         bool fullMessages,
                                         bool threadPerChannel)
{
  initializeStruct(&profileRegistrationStruct, profile,
		   uri, listenerModes, initiatorModes, 
		   fullMessages, threadPerChannel);
}

ProfileRegistration::ProfileRegistration(struct _profile_registration *pr)
{
  memcpy(&profileRegistrationStruct, pr, sizeof(profileRegistrationStruct));
}

void ProfileRegistration::initializeStruct(struct _profile_registration *pr, 
                                           Profile *profile,
                                           char *uri,
                                           char *imodes,
                                           char *lmodes,
                                           bool messagesOnly,
                                           bool threadPerChannel)
{
  assert(uri != NULL);

  pr->uri = uri;
  pr->next = NULL;
  pr->initiator_modes = imodes;
  pr->listener_modes = lmodes;
  pr->full_messages = messagesOnly;
  pr->thread_per_channel = threadPerChannel;
  
  // Use the default Profile handlers (which virtually call the REAL ones...)
  profile->fillInCallbacks(pr);
}

ProfileRegistration::~ProfileRegistration()
{
  // (thanks compiler)
  // Should we delete the uri? (depends on if it's being newed...probably
  // a good thing - consider.
}
