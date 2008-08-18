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
// PropertyMap.cpp: implementation of the PropertyMap class.
//
//////////////////////////////////////////////////////////////////////

#include "Common.h"
#include "PropertyMap.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

PropertyMap::PropertyMap()
{}

PropertyMap::~PropertyMap()
{
	// Clear the table
}

char *PropertyMap::getProperty(char *propertyName)
{
	NameValuePairMap::iterator it = properties.find((string)propertyName);
	if(it != properties.end())
		return (char*)(*it).second;
	return NULL;
}

void PropertyMap::setProperty(char *propertyName, char *propertyValue, 
							  bool overwrite)
{
	if(overwrite)
		properties.erase((string)propertyName);
	properties.insert(NameValuePairMap::value_type((string)propertyName, 
													propertyValue));
}

unsigned int PropertyMap::getSize()
{
	return properties.size();
}
