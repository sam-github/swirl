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
// ProfileRegistry.cpp: implementation of the ProfileRegistry class.
//
//////////////////////////////////////////////////////////////////////

//#include "EchoProfile.h"
#include "ProfileRegistry.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ProfileRegistry::ProfileRegistry()
  :profileList(NULL), listUpdateRequired(false)
{
}

ProfileRegistry::~ProfileRegistry()
{
  // Registry is created by default, no need to delete it
  // BUT
  // We do need to iterate through our ProfileRegistrations
  // and eliminate them TOD
  lock.lock();
  ProfileRegistrationMap::iterator it = registry.begin(); 
  ProfileRegistration *temp = NULL;
  while(it != registry.end())
    {
      temp = (ProfileRegistration*)(*it).second;
      delete(temp);
      it++;
    }
  profileList = NULL;
  lock.unlock();
}

ProfileRegistration *ProfileRegistry::getProfileRegistration(char *uri)
{
  lock.lock();
  ProfileRegistrationMap::iterator it = registry.find((string)uri);
  lock.unlock();
  if(it != registry.end())
    return (ProfileRegistration*)(*it).second;
  else
    return NULL;
}

void ProfileRegistry::addProfileRegistration(ProfileRegistration *pr, 
					     bool replace)
{
  lock.lock();
  if(replace)
    {
      if(registry.find((string)pr->getUri()) != registry.end()) 
	  registry.erase((string)pr->getUri());
    }
  registry.insert(ProfileRegistrationMap::value_type((string)pr->getUri(), pr));
  listUpdateRequired = true;
  lock.unlock();
}

// A bit of ugliness we have to do to deal with our separation/encapsulation
// in ProfileRegistration and ProfileListener to prepare a 'list' of structs
// for the wrapper.
struct _profile_registration *ProfileRegistry::getProfileList()
{
  if(!listUpdateRequired)
    return profileList;
  
  //dprintf("Getting the list...\n");
  struct _profile_registration *currentPRS = NULL, *nextPRS = NULL;
  ProfileRegistration *currentPR = NULL, *nextPR = NULL;

  // Kind of ugly TODO make it one-pass instead of two, if that's possible
  lock.lock(); 
  ProfileRegistrationMap::iterator it = registry.begin(); 
  ProfileRegistration *temp = NULL;
  //dprintf("Getting the list...\n");
  while(it != registry.end())
   {
      // Get the first ProfileRegistration (wrapping class)
      // and encapsulated struct, tracking the first in the chain..
     //dprintf("Getting the first element...\n");
     currentPR = (ProfileRegistration*)(*it).second;
     currentPRS = currentPR->getProfileRegistrationStruct();
     assert(currentPRS != NULL);
     //dprintf("Got PRS\n");
     if(profileList == NULL)
       profileList = currentPRS;
     
     // Get the next one
     it++;
     //dprintf("Got next %d\n", it == registry.end());
     if(it != registry.end())
       {
	 nextPR = (ProfileRegistration*)(*it).second;
	 nextPRS = nextPR->getProfileRegistrationStruct();
	}
     currentPRS->next = nextPRS;
     //dprintf("CurrentPRS->next = %x\n", nextPRS);
     nextPRS = NULL;
     nextPR = NULL;
   }
  //dprintf("Getting the list...\n");
  lock.unlock();
  return profileList;
}
