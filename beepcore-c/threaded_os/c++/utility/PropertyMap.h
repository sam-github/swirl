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
// PropertyMap.h: interface for the PropertyMap class.
//
// Set semantics are 'replace' by default, but can be overriden 
// to suit the developer's preferences.
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_PROPERTYMAP_H__E41D37DF_F45D_4B32_80D3_B44470C6A175__INCLUDED_)
#define AFX_PROPERTYMAP_H__E41D37DF_F45D_4B32_80D3_B44470C6A175__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifdef WIN32
#pragma warning(disable:4786)
#include <windows.h>
#endif

#include <map>
#include <string>
using namespace std;
typedef map<string, char*> NameValuePairMap;

class PropertyMap  
{
public:
	PropertyMap();
	virtual ~PropertyMap();
	char *getProperty(char *propertyName);
	void setProperty(char *propertyName, char *propertyValue, bool overwrite = true);
	unsigned int getSize();

private:
	NameValuePairMap properties;
};

#endif // !defined(AFX_PROPERTYMAP_H__E41D37DF_F45D_4B32_80D3_B44470C6A175__INCLUDED_)
