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
// EchoServer.h: interface for the EchoServer class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ECHOSERVER_H__060AFCC1_2FBF_451C_B0AF_B2D09D9F171F__INCLUDED_)
#define AFX_ECHOSERVER_H__060AFCC1_2FBF_451C_B0AF_B2D09D9F171F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Channel.h"
#include "Common.h"
#include "EchoProfile.h"
#include "Peer.h"
#include "Session.h"

class EchoServer  
{
public:
  EchoServer(){}
  virtual ~EchoServer(){}
  static int main(int argc,char *argv[]);
};

#endif // !defined(AFX_ECHOSERVER_H__060AFCC1_2FBF_451C_B0AF_B2D09D9F171F__INCLUDED_)
