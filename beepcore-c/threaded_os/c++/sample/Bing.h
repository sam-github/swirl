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
// Bing.h: interface for the Bing class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BING_H__68495851_C49A_496C_BB55_44E9B8B1A1BE__INCLUDED_)
#define AFX_BING_H__68495851_C49A_496C_BB55_44E9B8B1A1BE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BEEPException.h"

int PRIVACY_NONE = 0;
int PRIVACY_PREFERRED = 1;
int PRIVACY_REQUIRED = 2;

class Bing 
{
public:
  Bing(){
    port = "10288"; 
    count = 4; 
    size = 64; 
    privacy = PRIVACY_NONE;
  }
  virtual ~Bing(){}
  int run(int argc, char *argv[]) throw (BEEPException);
  static int main(int argc,char *argv[]);

private:
  char* host;
  char* port;
  int count;
  int size;
  int privacy;

  int parseArgs(int argc, char *argv[]);
  char* initSendData();
};

#endif // !defined(AFX_BING_H__68495851_C49A_496C_BB55_44E9B8B1A1BE__INCLUDED_)
