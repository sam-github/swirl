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
// ProfileRegistry.h: interface for the ProfileRegistry class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROFILEREGISTRY_H__AC839F61_2E7C_4EA3_891F_68BF8648D1FE__INCLUDED_)
#define AFX_PROFILEREGISTRY_H__AC839F61_2E7C_4EA3_891F_68BF8648D1FE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Common.h"
#include "Mutex.h"
#include "ProfileRegistration.h"

#include <map>
#include <string>
using namespace std;
typedef map<string, ProfileRegistration *> ProfileRegistrationMap;

class ProfileRegistry  
{
public:
	ProfileRegistry();
	virtual ~ProfileRegistry();

	ProfileRegistration *getProfileRegistration(char *uri);
	void addProfileRegistration(ProfileRegistration *pr, bool replace = true);
	PROFILE_REGISTRATION *getProfileList();
	unsigned int getSize(){return registry.size();}
 protected:
	void diggitydoo(){}

 private:
	bool listUpdateRequired;
	Mutex lock;
	ProfileRegistrationMap registry;
	PROFILE_REGISTRATION *profileList;
};

#endif // !defined(AFX_PROFILEREGISTRY_H__AC839F61_2E7C_4EA3_891F_68BF8648D1FE__INCLUDED_)
