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
// BEEPException.cpp: implementation of the BEEPException class.
//
//////////////////////////////////////////////////////////////////////

#include "BEEPException.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
/*
BEEPException::BEEPException(Session *s, DIAGNOSTIC *d)
  :message(NULL)
{
  assert(s != NULL && d != NULL);
  wrapper = s->getWrapper();
  diag = d;
  message = NULL;
}

BEEPException::BEEPException(char *msg)
  :wrapper(NULL), diag(NULL), message(msg)
{
}

BEEPException::BEEPException(Session *s, int code, char *message, char *lang)
  :message(NULL)
{
  assert(s != NULL);
  wrapper = s->getWrapper();
  diag = blw_diagnostic_new(wrapper, code, lang, message);
}

BEEPException::~BEEPException()
{
  // TODO fry the DIAGNOSTIC stuff via the appropriate call
  if(wrapper != NULL)
    blw_diagnostic_destroy(wrapper, diag);
  if(message != NULL)
  // TDOO - establish a convention for who kills the memory
    ;
}
*/
