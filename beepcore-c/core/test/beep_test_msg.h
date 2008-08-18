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
/*
 * $Id: beep_test_msg.h,v 1.3 2001/10/30 06:00:37 brucem Exp $
 *
 * beep_test.msg.h
 *
 * a structure of messages we use in various tests.
 */
#include <stddef.h>

struct beep_test_msg {char frametype, * testname, * testdata;} beep_msgs[] = {
  /* 
   * greeting messages: empty and populated
   */
  {'R', "greeting: empty unary:", "Content-Type: application/beep+xml\r\n\r\n<greeting />"}, 
  {'R', "greeting: no MIME:", "<greeting />"}, 
  {'R', "greeting: no MIME leading w/s:", "\r\n<greeting />"}, 
  {'R', "greeting: trailing whitespace:", "Content-Type: application/beep+xml\r\n\r\n<greeting />\r\n"},
  {'R', "greeting: leading whitespace:", "Content-Type: application/beep+xml\r\n\r\n<greeting />\r\n"},
  {'R', "greeting: empty binary:", "Content-Type: application/beep+xml\r\n\r\n<greeting></greeting>"}, 
  {'R', "greeting: empty w/attrib:", "Content-Type: application/beep+xml\r\n\r\n<greeting localize='eng/us' features = 'test features'/>"}, 
  {'R', "greeting: empty profile:", "Content-Type: application/beep+xml\r\n\r\n<greeting>\r\n   <profile uri='http://iana.org/beep/TLS' />\r\n</greeting>\r\n"},
  {'R', "greeting: multiple empty profiles:", "Content-Type: application/beep+xml\r\n\r\n<greeting>\r\n   <profile uri='http://iana.org/beep/TLS' />\r\n\r\n   <profile uri='http://iana.org/beep/SASL/OTP' />\r\n   <profile uri='http://iana.org/beep/FOO' /></greeting>\r\n"},
  
  /* start message transaction set 
   *
   * starts: 1 and N profiles.  one piggyback example
   * replies success: with and without piggyback
   * replies error:
   */
  {'M', "start: chan=1: 1 empty profile:", "Content-Type: application/beep+xml\r\n\r\n<start number='1'>\r\n   <profile uri='http://iana.org/beep/SASL/OTP' />\r\n</start>\r\n"},
  {'M', "start: chan=1: 2 empty profiles:", "Content-Type: application/beep+xml\r\n\r\n<start number='1'>\r\n   <profile uri='http://iana.org/beep/SASL/OTP' />\r\n   <profile uri='http://iana.org/beep/SASL/ANONYMOUS' />\r\n</start>\r\n"},
  {'M', "start: chan=2: 1 empty profile:", "Content-Type: application/beep+xml\r\n\r\n<start number='2'>\r\n   <profile uri='http://iana.org/beep/FOO' />\r\n</start>\r\n\r\n"},
  {'M', "start: chan=1: non-empty profile w/CDATA:", "Content-Type: application/beep+xml\r\n\r\n<start number='1'>\r\n   <profile uri='http://iana.org/beep/TLS'>\r\n       <![CDATA[<ready />]]>\r\n   </profile>\r\n</start>\r\n"},
  {'M', "start: chan=11: non-empty profile with b64 CDATA:", "<start number='11' ><profile uri='http://iana.org/beep/SASL/OTP' encoding='base64' >AQEBT25jZSB1cG9uIGEgbWlkbmlnaHQgZHJlYXJ5</profile></start>"},

  {'R', "confirm: empty profile:", "Content-Type: application/beep+xml\r\n\r\n<profile uri='http://iana.org/beep/SASL/OTP' />\r\n"},
  {'R', "confirm: profile w/CDATA:", "Content-Type: application/beep+xml\r\n\r\n<profile uri='http://iana.org/beep/TLS'>\r\n    <![CDATA[<proceed />]]>\r\n</profile>\r\n"},

  {'E', "error: with text:", "Content-Type: application/beep+xml\r\n\r\n<error code='550'>all requested profiles are\r\nunsupported</error>\r\n"},
  {'E', "error: with text and entities:", "Content-Type: application/beep+xml\r\n\r\n<error code='501'>number attribute\r\nin &lt;start&gt; element must be odd-valued</error>\r\n"},

  /* close message set (error handled above) */
  {'M', "close: no channel:", "Content-Type: application/beep+xml\r\n\r\n<close code='200' />\r\n"},
  {'M', "close: channel 0:", "Content-Type: application/beep+xml\r\n\r\n<close number='0' code='200' />\r\n"},
  {'M', "close: channel 1:", "Content-Type: application/beep+xml\r\n\r\n<close number='1' code='200' />\r\n"},
  {'M', "close: with text:", "Content-Type: application/beep+xml\r\n\r\n<close number='1' code='200'>Session timout.</close>\r\n"},

  {'R', "ok: unary:", "Content-Type: application/beep+xml\r\n\r\n<ok />\r\n"},
  {'R', "ok: binary:", "Content-Type: application/beep+xml\r\n\r\n<ok></ok>\r\n"},
  /* elephant in karthoum */
  {' ', NULL, NULL}
};

