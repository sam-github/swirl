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
// ProfileRegistration.h: interface for the ProfileRegistration class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROFILEREGISTRATION_H__2DA8A747_F26E_43E6_960A_52BE1C2AF869__INCLUDED_)
#define AFX_PROFILEREGISTRATION_H__2DA8A747_F26E_43E6_960A_52BE1C2AF869__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#pragma warning( disable:4786 )

// TODO Define some constants for encoding types based on their usage
// in lower layers whenever that becomes more clear.

#include "bp_wrapper.h"

class Profile;
#include "Common.h"

class ProfileRegistration  
{
  friend class ProfileRegistry;
  friend class Session;
 public:
  static const char PLAINTEXT[];

  ProfileRegistration(char *uri, Profile *p,
		      char *listener_modes = (char*)PLAINTEXT, 
		      char *initiator_modes = (char*)PLAINTEXT,
		      bool fullMessages = false, bool threadPerChannel = false);  
  ProfileRegistration(struct _profile_registration *pr);

  virtual ~ProfileRegistration();
  char *getUri(){return profileRegistrationStruct.uri;}
  char *getListenerModes(){return profileRegistrationStruct.listener_modes;}
  char *getInitiatorModes(){return profileRegistrationStruct.initiator_modes;}
  void setListenerModes(char *modes){profileRegistrationStruct.listener_modes = modes;}
  void setInitiatorModes(char *modes){profileRegistrationStruct.initiator_modes = modes;}

  void *getPRegStructRef(){return profileRegistrationStruct.externalProfileReference;}
 protected:
  PROFILE_REGISTRATION *getProfileRegistrationStruct()  
    {return &profileRegistrationStruct;}

  static void initializeStruct(PROFILE_REGISTRATION *pr, Profile *p,
			       char *uri, char *imodes, char *lmodes, 
			       bool messagesOnly, bool threadPerChannel);
 private:
  PROFILE_REGISTRATION profileRegistrationStruct;
};

#endif // !defined(AFX_PROFILEREGISTRATION_H__2DA8A747_F26E_43E6_960A_52BE1C2AF869__INCLUDED_)
