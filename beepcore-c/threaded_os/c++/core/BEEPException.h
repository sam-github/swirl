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
// BEEPException.h: interface for the BEEPException class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BEEPEXCEPTION_H__FCBF4DA3_FEE4_490C_9686_FEC8EE131DDB__INCLUDED_)
#define AFX_BEEPEXCEPTION_H__FCBF4DA3_FEE4_490C_9686_FEC8EE131DDB__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//#include "bp_wrapper.h"
//#include "Session.h"

class BEEPException  
{
 public:
  BEEPException(){}
  /*
    BEEPException(Session *s, DIAGNOSTIC *d);
    // TODO use unknown err code (what was it again?)
    BEEPException(Session *s, int code = 0, char *message = NULL, char *lang = NULL);
    // Last resort - TODO flesh otu with code/lang 
    BEEPException(char *msg);
  */
  virtual ~BEEPException(){}
 private:
  /*
  char *message;
  DIAGNOSTIC *diag;
  WRAPPER *wrapper;
  */
};

#endif // !defined(AFX_BEEPEXCEPTION_H__FCBF4DA3_FEE4_490C_9686_FEC8EE131DDB__INCLUDED_)
