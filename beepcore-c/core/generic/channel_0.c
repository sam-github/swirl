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
 * $Id: channel_0.c,v 1.8 2002/03/18 21:48:39 sweetums Exp $
 *
 * channel_0.c
 *
 * Funtionality specific to channel 0 processing, including 
 * parsing and formatting messages for channel 0.
 *
 */

char * __channel_0_c_ver__ = "$Id: channel_0.c,v 1.8 2002/03/18 21:48:39 sweetums Exp $";

#include "CBEEPint.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#ifndef WIN32
#include <strings.h>
#else
#include <windows.h>
#endif

#include "channel_0.h"

#include "base64.h"
#include "cbxml_entities.h"
#include "../../utility/xml_parse_constants.h"

#define MIN(x,y) ((x<y)?x:y)
#define MAX(x,y) ((x>y)?x:y)

/*   ------------------------- 80 char laylout line -----------------------   */

/*
 * Various utility functions.
 */
int chan0_base64_isneeded(struct profile *);

/*
 * channel 0 message formatting function prototypes and string defines.
 */
char *  chan0_msg_fmt_start (struct session *, struct chan0_msg *);
char *  chan0_msg_fmt_close (struct session *, struct chan0_msg *);
char *  chan0_msg_fmt_greet (struct session *, struct chan0_msg *);
char *  chan0_msg_fmt_error (struct session *, struct chan0_msg *);
char *  chan0_msg_fmt_ok (struct session *, struct chan0_msg *);
char *  chan0_msg_fmt_profile (struct session *, struct chan0_msg *);

void thingie_nmlize_sprintf(char *data, char **dest, char **tmpbuf, char *fmt);
void thingie_nmlize_sizer(char *data, char *fmt, int *max, int *size);


#define CHAN0_ELEM_FMT_UNARY_CLOSE "/>"

#define CHAN0_ELEM_FMT_GREET_HEAD  "<greeting "
#define CHAN0_ELEM_FMT_GREET_TAIL  ">%s</greeting>"
#define CHAN0_ELEM_FMT_ATTRIB_FEAT "features='%s' "
#define CHAN0_ELEM_FMT_ATTRIB_LOCL "localize='%s' "

#define CHAN0_ELEM_FMT_START_HEAD  "<start number='%s' "
#define CHAN0_ELEM_FMT_START_TAIL  ">%s</start>"
#define CHAN0_ELEM_FMT_ATTRIB_SERVER "server_name='%s' "

#define CHAN0_ELEM_FMT_PROFILE_HEAD  "<profile uri='%s' "
#define CHAN0_ELEM_FMT_PROFILE_TAIL  ">%s</profile>"
#define CHAN0_ELEM_FMT_ATTRIB_ENCODING "encoding='base64' "

#define CHAN0_ELEM_FMT_ERROR_HEAD  "<error code='%3d' "
#define CHAN0_ELEM_FMT_ERROR_TAIL  ">%s</error>"

#define CHAN0_ELEM_FMT_CLOSE_HEAD  "<close number='%d' code='%3d' "
#define CHAN0_ELEM_FMT_CLOSE_TAIL  ">%s</close>"

#define CHAN0_ELEM_FMT_OK          "<ok />"

#define CHAN0_ELEM_FMT_ATTRIB_LANG "xml:lang='%s' "

/*
 * Parsing defines and constructs
 */
enum c0parse_state {C0_SOF, C0_MIMEH, C0_SOM, C0_EOM,
		    C0_ELEMNAMESTART, C0_ELEMNAME, C0_ELEMCLOSE, C0_ELEMUNARYCLOSE,
		    C0_ELEMSPECIAL, C0_ELEMCOMMENT, 
		    C0_ELEM_GREETING, C0_ELEM_START, C0_ELEM_CLOSE, 
		    C0_ELEM_ERROR, C0_ELEM_PROFILE, C0_ELEM_PROFILE_START, C0_ELEM_OK, 
		    C0_ATT_SERVERNAME, C0_ATT_LOCALIZE, C0_ATT_FEATURES, 
		    C0_ATT_URI, C0_ATT_ENCODING, C0_ATT_NUMBER, 
		    C0_CDATA, C0_PCDATA, C0_PCDATABEGIN, C0_COMMENT,
		    C0_LITERAL, C0_LITERAL_EQ, C0_LITERAL_CT, C0_LITERAL_CT_EQ, 
		    C0_SQLITERAL, C0_DQLITERAL, C0_SQLITERAL_CT, C0_DQLITERAL_CT, 
		    C0_MATCH_SPECIFIC, C0_WHITESPACE_UNTIL, C0_ATTRIB_EQAL, C0_ATTRIB_QUOTE,
                    C0_ERROR};


#define CHAN0_MIME_HEADER          "Content-Type: application/beep+xml\r\n\r\n"

char * chan0_mime_header = CHAN0_MIME_HEADER;

#ifdef WIN32
#undef ERROR
#endif

#define ERROR(message) {printf("%s\n\nFatal.\n", message); state[stateidx] = C0_ERROR; currbyte = segsize;}

/*
 * chan0_parse
 *
 * Parse a complete chan0 message frame list into a chan0 struct.
 * This is expected to return a pointer to a complex memory structure
 * which is contiguous and the result of a single call to session_alloc.
 * All of the pointers within the chan0 message struct will point
 * into this block.
 * 
 * There are some cases wehre it is possbile to parse the message in 
 * a single pass.  Channel 0 messages can be parsed in a single pass 
 * in all but one case which is a frame containing the "start" element, 
 * this is handled in 2 passes.
 */
#define CHAN0_PARSE_STATE_MAX_DEPTH    256

struct chan0_msg * chan0_msg_parse (struct session * session,
					struct frame * chan0_frame) {
  struct frame * thisframe;
  struct chan0_msg mychan0_msg, * mychan0_ptr = NULL;
  struct profile * profilelist = NULL, * thisprofile, * newprofile = NULL;
  struct profile ** current_profile = NULL;
  char ** current_string = NULL;

  char * currchar, * matchstr, * matchfailstr, * thischar;
  char * stringseg;
  long segsize, currbyte, stringsegsize, chan0msgsize, profileC;
  long parselevel, stridx, matchstridx;

  long  state[CHAN0_PARSE_STATE_MAX_DEPTH], stateidx = 0;
  char * elementnamestack[CHAN0_PARSE_STATE_MAX_DEPTH];

  int * current_counter = NULL, * current_offset = NULL;
  int features_offset = 0, features_len = 0;
  int localize_offset = 0, localize_len = 0;
  int server_name_offset = 0, server_name_len = 0;
  int number_offset = 0, number_len = 0;
  int code_offset = 0, code_len = 0;
  int lang_offset = 0, lang_len = 0;
  int message_offset = 0, message_len = 0;
  int uri_offset = 0, uri_len = 0;
  int encoding_offset = 0, encoding_len = 0;
  int piggy_offset = 0, piggy_len = 0;

  /*
   * Life is easier if we assume a single frame and if we can munge the data
   * on the fly, so we'll make that happen.
   *
   * We'll also use this as our work area and abuse it as needed by 
   * null terminating strings and such in place so they can be copied later.
   */

  if (NULL == (thisframe = blu_frame_aggregate(session, chan0_frame))) {
    FAULT(session, 451, "chan0_parse: memory allocation failed.");
    return(NULL);
  }


  /* start on the basic init of our return data */   
  memset(&mychan0_msg, 0, sizeof(struct chan0_msg));
  mychan0_msg.session = chan0_frame->session;
  mychan0_msg.channel_number = chan0_frame->channel_number;
  mychan0_msg.message_number = chan0_frame->message_number;
  
  switch (chan0_frame->msg_type) {
  case 'M': 
    mychan0_msg.op_ic = 'i';
    break;
  case 'R': 
    mychan0_msg.op_ic = 'c';
    break;
  case 'E': 
    mychan0_msg.op_sc = 'e';
    mychan0_msg.op_ic = 'c';
    break;
  default: 
    FAULT(session, 553, "Invalid message type received on channel 0");
    break;
  }

  /* First pass parsing */ 
  state[stateidx] = C0_SOF;
  parselevel = 0;
  stringsegsize = 0;
  profileC = 0;

  currchar = thisframe->payload;
  segsize = thisframe->size;

  for (currbyte = 0; currbyte < segsize; currbyte++, currchar++) {
    if (!(XML_UTF8_charset[(int)currchar[0]] & XML_CHAR)) {
      FAULT(session, 501, "Invalid message: non-XML supported character");
      state[stateidx] = C0_ERROR; 
    }
    switch (state[stateidx]) {
    case C0_MATCH_SPECIFIC:
      if (currchar[0] == matchstr[matchstridx++]) {
	if (0 == matchstr[matchstridx]) {
	  stateidx--;                          /* pop state to success */
	}
      } else {
	FAULT(session, 500, matchfailstr);
	state[stateidx] = C0_ERROR; 
      }
      break;
    case C0_SOF:                               /* start of frame */
      if ('C' == currchar[0]) {
	state[stateidx++] = C0_SOM;
	state[stateidx] = C0_MATCH_SPECIFIC;
	matchstr = chan0_mime_header;
	matchfailstr = "Invalid message: Mime header not correct.";
	matchstridx = 1;
	break;
      }
    case C0_SOM:                               /* start of message */
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	state[stateidx] = C0_SOM;	  
	break;
      }	
      switch (currchar[0]) {
      case '<':
	state[stateidx++] = C0_EOM;
	state[stateidx] = C0_ELEMNAMESTART;
	break;
      default:
	FAULT(session, 500, "Invalid message: does not start with XML tag or MIME header.");
	state[stateidx] = C0_ERROR; 
	break;
      }
      break;
    case C0_EOM:
      /* 
       * we should only encounter this on the way down... 
       * if we had trailer support it would go here somewhere.
       * whitespaces is fine at EOM since it may precede trailer.
       */
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	
      FAULT(session, 500, "Invalid message: trailing garbage");
      state[stateidx] = C0_ERROR; 
      break;
    case C0_MIMEH:
      /*
	if (currchar[0] == chan0_mime_header[stridx++]) {
	if (currchar[0] != matchstr[matchstridx++]) {
	matchstridx = 0;
	} else { 
	if (0 == matchstr[matchstridx]) { 
	state[stateidx] = C0_SOM; 
	} 
	} 
	} else { 
	FAULT(session, 500, "Invalid message: Mime header not correct."); 
	}
      */
      break;
    case C0_ELEMNAMESTART: {
      matchstridx = 1;
      switch (currchar[0]) {
      case '/': 
	stateidx -= 2;                  /* pop the current state and the PCDATA state */
	parselevel--;
	matchstridx = 0;
	matchstr = elementnamestack[stateidx];
	matchfailstr = "Invalid message: Unexpected XML close tag";
	state[stateidx++] = C0_ELEMCLOSE;
	state[stateidx] = C0_MATCH_SPECIFIC;
	/* this is custom conde to handle closing specific element types */
	if (matchstr && (NULL != newprofile) && (0 == strcmp("profile", matchstr))) {
	  /* we are processing a profile element and need to close it out*/
	  if (newprofile->uri) {
	    newprofile->uri[uri_len] = '\0';
	  } else {	    
	    FAULT(session, 501, "chan0_parse: profile element missing required uri");
	    state[stateidx] = C0_ERROR; 
	  }
	  if (newprofile->piggyback) {
	    newprofile->piggyback_length -= 1;
	    newprofile->piggyback[newprofile->piggyback_length] = '\0'; 
	  }
	  if (encoding_offset) {
	    switch (thisframe->payload[encoding_offset]) {
	    case 'n':
	    case 'N':
	    case '\0':
	      newprofile->encoding = (char)0;
	      break;
	    default:
	      newprofile->encoding = 'Y';
	      break;
	    }
	  }
	  newprofile = NULL;
	}
	break;
      case '!': 
	state[stateidx] = C0_ELEMSPECIAL;
	break;
      default:
	matchfailstr = "Invalid message: unexpected element.";

	switch  (parselevel) {
	case 0:
	  switch (currchar[0]) {
	  case 'g':
	  case 's':
	  case 'c':
	  case 'p':
	  case 'o':
	  case 'e':
	    break;
	  default:
	    FAULT(session, 553, matchfailstr);
	    state[stateidx] = C0_ERROR; 
	    break;
	  }
	  break;
	case 1:
	  if ('p' != currchar[0]) {
	    FAULT(session, 553, matchfailstr);
	    state[stateidx] = C0_ERROR; 
	    break;
	  }
	  break;
	default:
	  FAULT(session, 553, matchfailstr);
	  state[stateidx] = C0_ERROR; 
	  break;
	}	      
	if (C0_ERROR == state[stateidx])
	  break;

	switch (currchar[0]) {
	case 's': 
	  if (!(0 == parselevel)) {
	    FAULT(session, 501, matchfailstr);
	    state[stateidx] = C0_ERROR;
	    break;
	  }	      
	  mychan0_msg.op_sc = 's';
	  elementnamestack[stateidx] = matchstr = "start";
	  state[stateidx++] = C0_ELEM_START;
	  state[stateidx] = C0_MATCH_SPECIFIC;
	  break;
	case 'c': 
	  if (!(0 == parselevel)) {
	    FAULT(session, 501, matchfailstr);
	    state[stateidx] = C0_ERROR;
	    break;
	  }	      
	  mychan0_msg.op_sc = 'c';
	  elementnamestack[stateidx] = matchstr = "close";
	  state[stateidx++] = C0_ELEM_CLOSE;
	  state[stateidx] = C0_MATCH_SPECIFIC;
	  break;
	case 'g': 
	  if (!(0 == parselevel)) {
	    FAULT(session, 501, matchfailstr);
	    state[stateidx] = C0_ERROR;
	    break;
	  }	      
	  mychan0_msg.op_sc = 'g';
	  elementnamestack[stateidx] = matchstr = "greeting";
	  state[stateidx++] = C0_ELEM_GREETING;
	  state[stateidx] = C0_MATCH_SPECIFIC;
	  break;
	case 'p': 
	  if (0 == mychan0_msg.op_sc) 
	    mychan0_msg.op_sc = 's';
	  elementnamestack[stateidx] = matchstr = "profile";
	  state[stateidx++] = C0_ELEM_PROFILE_START;
	  state[stateidx] = C0_MATCH_SPECIFIC;
	  profileC++;
	  break;
	case 'o': 
	  if (!(0 == parselevel)) {
	    FAULT(session, 501, matchfailstr);
	    state[stateidx] = C0_ERROR;
	    break;
	  }	      
	  mychan0_msg.op_sc = 'c';
	  elementnamestack[stateidx] = matchstr = "ok";
	  state[stateidx++] = C0_ELEM_OK;
	  state[stateidx] = C0_MATCH_SPECIFIC;
	  break;
	case 'e': 
	  if (!(0 == parselevel)) {
	    FAULT(session, 501, matchfailstr);
	    state[stateidx] = C0_ERROR;
	    break;
	  }
          if (0 == mychan0_msg.op_sc) {
            mychan0_msg.op_sc = 'e';	
            mychan0_msg.op_ic = 'c';	
	  }      
	  elementnamestack[stateidx] = matchstr = "error";
	  state[stateidx++] = C0_ELEM_ERROR;
	  state[stateidx] = C0_MATCH_SPECIFIC;
	  break;
	default: 
	  FAULT(session, 501, matchfailstr);
	  state[stateidx] = C0_ERROR;
	  break;
	}
      } /* special (!/) case switch  */
      break;
    }
    case C0_WHITESPACE_UNTIL:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }
      if (matchstr[0]) {
	for (thischar = matchstr; thischar[0]; thischar++) {
	  if (currchar[0] == thischar[0]) {
	    /* 
	     * OK this is probably evil.  Want to stay on the current
	     * character. Will be undone by the currchar++ and currbyte++
	     * in the for(;;).
	     */
	    stateidx--;                          /* pop state to previous */
	    currbyte--; 
	    currchar--; 
	    break;
	  }
	}
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
      } else {
	/* This is still evil */
	stateidx--;                          /* pop state to success */
	currbyte--; 
	currchar--; 
      }
      break;
    case C0_LITERAL:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	

      if (current_offset) {
	*current_offset = currbyte;
	current_offset = NULL;
      }
      switch (currchar[0]) {
      case '\'': 
	state[stateidx] = C0_SQLITERAL;
	break;
      case '"': 
	state[stateidx] = C0_DQLITERAL;
	break;
      default: 
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
	break;
      }
      break;
    case C0_LITERAL_CT:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	
      switch (currchar[0]) {
      case '\'': 
	state[stateidx] = C0_SQLITERAL_CT;
	break;
      case '"': 
	state[stateidx] = C0_DQLITERAL_CT;
	break;
      default: 
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
	break;
      }
      if (C0_ERROR != state[stateidx]) {
	if (current_string) {
	  *current_string = currchar + 1;
	  current_string = NULL;
	}
	if (current_offset) {
	  *current_offset = currbyte + 1;
	  current_offset = NULL;
	}
      }
      break;
    case C0_SQLITERAL_CT:  
      stringsegsize++;         /* automatically add 1 for the \0 as well */ 
      if (current_counter) 
	(*current_counter)++;
    case C0_SQLITERAL: 
      if ('\'' == currchar[0]) {	      
	stateidx--;            /* pop back to previous state */
	if (current_counter) {
	  /* $@$@$@$ May be evil to null terminate here, but we do. */
	  currchar[0] = '\0';

	  (*current_counter)--;
	  current_counter = NULL;
	}
      } 
      break;
    case C0_DQLITERAL_CT:
      stringsegsize++;         /* automatically add 1 for the \0 as well */ 
      if (current_counter) 
	(*current_counter)++;
    case C0_DQLITERAL: 
      if ('"' == currchar[0]) {	      
	stateidx--;            /* pop back to previous state */
	if (current_counter) {
	  /* $@$@$@$ May be evil to null terminate here, but we do. */
	  currchar[0] = '\0';

	  (*current_counter)--;
	  current_counter = NULL;
	}
      } 
      break;	  
    case C0_ATTRIB_EQAL:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }
      switch (currchar[0]) {
      case '=': 
	stateidx--;
	break;
      default: 
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
	break;
      }
      break;
    case C0_ATTRIB_QUOTE:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	
      switch (currchar[0]) {
      case '\'': 
      case '"': 
	/* This is still evil  -- need to forward this char to LOWER state */
	stateidx--;                          /* pop state to success */
	currbyte--; 
	currchar--; 
	break;
      default: 
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
	break;
      }
      break;
    case C0_ELEMCLOSE:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	
    case C0_ELEMUNARYCLOSE:
      if ('>' == currchar[0]) {
	stateidx--;                          
      } else {
	FAULT(session, 501, "Invalid message: XML parse error");
	state[stateidx] = C0_ERROR;
      }
      break;
    case C0_ELEM_GREETING:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	
      matchfailstr = "Invalid message: greeting, invalid attribute.";
      matchstridx = 1;
      switch (currchar[0]) {
      case '/': 
	state[stateidx] = C0_ELEMUNARYCLOSE;
	break;
      case '>': 
	state[++stateidx] = C0_PCDATABEGIN;
	break;
      case 'f': 
	current_offset = &features_offset;
	current_counter = &features_len;
	state[++stateidx] = C0_LITERAL_CT;
	state[++stateidx] = C0_ATTRIB_EQAL;    
	state[++stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "features";
	break;
      case 'l': 
	current_offset = &localize_offset;
	current_counter = &localize_len;
	state[++stateidx] = C0_LITERAL_CT;
	state[++stateidx] = C0_ATTRIB_EQAL;    
	state[++stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "localize";
	break;
      default: 
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
	break;
      }
      break;
    case C0_ELEM_START:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	
      matchfailstr = "Invalid message: start, invalid attribute.";
      matchstridx = 1;
      switch (currchar[0]) {
      case '/': 
	state[stateidx] = C0_ELEMUNARYCLOSE;
	break;
      case '>': 
	state[++stateidx] = C0_PCDATABEGIN;
	break;
      case 's': 
	current_offset = &server_name_offset;
	current_counter = &server_name_len;
	state[++stateidx] = C0_LITERAL_CT;
	state[++stateidx] = C0_ATTRIB_EQAL;    
	state[++stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "server_name";
	break;
      case 'n': 
	current_offset = &number_offset;
	current_counter = &number_len;
	state[++stateidx] = C0_LITERAL_CT;
	state[++stateidx] = C0_ATTRIB_EQAL;    
	state[++stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "number";
	break;
      default: 
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
	break;
      }
      break;
    case C0_ELEM_ERROR:
    case C0_ELEM_CLOSE:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	

      current_offset = &message_offset;
      current_counter = &message_len;

      switch (state[stateidx]) {
      case C0_ELEM_ERROR:
	matchfailstr = "Invalid message: error, invalid attribute.";
	break;
      case C0_ELEM_CLOSE:
	matchfailstr = "Invalid message: close, invalid attribute.";
	break;
      }
      matchstridx = 1;
      switch (currchar[0]) {
      case '/': 
	state[stateidx] = C0_ELEMUNARYCLOSE;
	break;
      case '>': 
	state[++stateidx] = C0_PCDATABEGIN;
	break;
      case 'n': 
	if (C0_ELEM_CLOSE != state[stateidx]) {
	  FAULT(session, 501, matchfailstr);
	  state[stateidx] = C0_ERROR;
	}
	current_offset = &number_offset;
	current_counter = &number_len;
	state[++stateidx] = C0_LITERAL_CT;
	state[++stateidx] = C0_ATTRIB_EQAL;    
	state[++stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "number";
	break;
      case 'c': 
	current_offset = &code_offset;
	current_counter = &code_len;
	state[++stateidx] = C0_LITERAL_CT;
	state[++stateidx] = C0_ATTRIB_EQAL;    
	state[++stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "code";
	break;
      case 'x': 
	current_offset = &lang_offset;
	current_counter = &lang_len;
	state[++stateidx] = C0_LITERAL_CT;
	state[++stateidx] = C0_ATTRIB_EQAL;    
	state[++stateidx] = C0_MATCH_SPECIFIC;
 	matchstr = "xml:lang";
	break;
      default: 
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
	break;
      }
      break;
    case C0_ELEM_PROFILE_START:
      state[stateidx] = C0_ELEM_PROFILE;
      if (NULL == (newprofile = session->malloc(sizeof(struct profile)))) {
	FAULT(session, 451, "Memory allocation failed.");
	state[stateidx] = C0_ERROR;
	break;
      }
      memset(newprofile, 0, sizeof(struct profile));
      if (NULL == profilelist) {
	profilelist = newprofile;
	thisprofile = newprofile;
      } else {
	thisprofile->next = newprofile;
	thisprofile = newprofile;
      }
      current_profile = &newprofile;
      /* no break here */
    case C0_ELEM_PROFILE:
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	
      matchfailstr = "Invalid message: profile, invalid attribute.";
      matchstridx = 1;
      switch (currchar[0]) {
      case '/': 
	state[stateidx] = C0_ELEMUNARYCLOSE;
	break;
      case '>': 
	current_string = &(newprofile->piggyback);
	current_offset = &piggy_offset;
	current_counter = &(newprofile->piggyback_length);
	state[++stateidx] = C0_PCDATABEGIN;
	break;
      case 'u': 
	current_offset = &uri_offset;
	current_counter = &uri_len;
	current_string = &(newprofile->uri);

	state[++stateidx] = C0_LITERAL_CT;
	state[++stateidx] = C0_ATTRIB_EQAL;    
	state[++stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "uri";
	break;
      case 'e': 
	current_offset = &encoding_offset;
	current_counter = &encoding_len;

	state[++stateidx] = C0_LITERAL_CT;
	state[++stateidx] = C0_ATTRIB_EQAL;    
	state[++stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "encoding";
	break;
      default: 
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
	break;
      }
      break;
    case C0_ELEM_OK: 
      if (XML_S & XML_UTF8_charset[(int)currchar[0]]) {
	break;
      }	
      matchfailstr = "Invalid message: ok, invalid attribute.";
      matchstridx = 1;
      switch (currchar[0]) {
      case '/': 
	state[stateidx] = C0_ELEMUNARYCLOSE;
	break;
      case '>': 
	state[++stateidx] = C0_PCDATABEGIN;
	break;
      default: 
	FAULT(session, 501, matchfailstr);
	state[stateidx] = C0_ERROR;
	break;
      }
      break;
    case C0_ELEMSPECIAL: 
      matchfailstr = "Invalid message: XML not well formed";
      matchstridx = 2;

      switch (currchar[0]) {
      case '-': 
	FAULT(session, 501, "Invalid message: XML comment not supported.");
	state[stateidx] = C0_ERROR;
	stringsegsize += 4;
	state[stateidx++] = C0_COMMENT;
	state[stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "!--";
	break;
      case '[': 
	stringsegsize += 7;
	if (current_counter) 
	  (*current_counter) += 7;
	state[stateidx++] = C0_CDATA;
	state[stateidx] = C0_MATCH_SPECIFIC;
	matchstr = "![CDATA[";
	break;
      default:
	break;
      }
      break;
    case C0_CDATA: 
      /* 
       * looking for 2 or more ] followed by > to return to PCDATA 
       * using stridx variable C0_MATCH_SPECIFIC will not be called 
       */
      stringsegsize++;
      if (current_counter) 
	(*current_counter)++;
      switch (currchar[0]) {
      case ']':
	stridx++;
	break;
      case '>':
	if (2 <= stridx) {
	  stateidx--;  /* pop back to PCDATA */
	} else {
	  stridx = 0;
	}
	break;
      default:
	stridx = 0;
      }
      break;
    case C0_PCDATABEGIN:
      parselevel++;
      state[stateidx] = C0_PCDATA;
      if (current_string) {
	*current_string = currchar;
	current_string = NULL;
      }
      if (current_offset) {
	*current_offset = currbyte;
	current_offset = NULL;
      }
    case C0_PCDATA:
      stringsegsize++;
      if (current_counter) 
	(*current_counter)++;
      if ('<' == currchar[0]) {
	state[++stateidx] = C0_ELEMNAMESTART;
      }
      break;
    case C0_COMMENT: 
      FAULT(session, 451, "Programmer error: Comment tag not supported");
      state[stateidx] = C0_ERROR;
      break;
    default: 
      FAULT(session, 451, "Programmer error: invalid parser state");
    case C0_ERROR:
      currbyte = segsize;
      break;
    }
  }

  if (C0_EOM != state[stateidx]) {
    /* end of frames reached without complete message */
    FAULT(session, 501, "Invalid message: XML not well formed");
  }

  /* 
   * Valid first pass parse with static data 
   * so now to allocate the frame and populate.
   *
   * We are allocating here the chan0_msg, all the 
   * addidtional structures we need, and space for all of
   * the string data in the frame that we would want to copy.
   * THis tring area will also be our work area for in-place
   * transformations and such.
   */

  chan0msgsize = sizeof(struct chan0_msg) + stringsegsize + 1;
  chan0msgsize += profileC * sizeof(struct profile);
  if ('e' == mychan0_msg.op_sc || 'c' == mychan0_msg.op_sc)
    chan0msgsize += sizeof(struct diagnostic);

  if (NULL != (mychan0_ptr = session->malloc(chan0msgsize))) {
    memset(mychan0_ptr, 0, chan0msgsize);
  }

  /* 
   * populate return structure 
   */
  memcpy(mychan0_ptr, &mychan0_msg, sizeof(struct chan0_msg));

  /* figure out the various pointers and populate structs*/

  stringseg = (char *)mychan0_ptr + sizeof(struct chan0_msg);
  if ('e' == mychan0_msg.op_sc || 'c' == mychan0_msg.op_sc) {
    /*  if we have a close or error */
    if (code_len) {
      mychan0_ptr->error = (struct diagnostic *)stringseg;
      stringseg += sizeof(struct diagnostic);

      if (code_len) {
	thisframe->payload[code_offset + code_len] = '\0';
	mychan0_ptr->error->code = atol(thisframe->payload + code_offset);
      }
#if 1
      /* changed to "--" to avoid overrun...  */
      if (--message_len > 0) {
#else
      if (message_len > 0) {
#endif
	mychan0_ptr->error->message = stringseg;
	strncpy(stringseg, thisframe->payload + message_offset, message_len);
	if (cbxml_include_isneeded(stringseg))
	  stringseg += cbxml_include_inplace(stringseg) + 2;
	else
	  stringseg += message_len + 2; 
      }
      if (lang_len) {
	mychan0_ptr->error->lang = stringseg;
	strncpy(stringseg, thisframe->payload + lang_offset, lang_len);
	if (cbxml_include_isneeded(stringseg))
	  stringseg += cbxml_include_inplace(stringseg) + 2;
	else
	  stringseg += lang_len + 2; 
      }
    }
  } else if (profileC) {
    /* may be true for greeting and profile (start message response) */
    mychan0_ptr->profile = (struct profile *)stringseg;
    stringseg += sizeof(struct profile) * profileC;
    
    newprofile = mychan0_ptr->profile;
    thisprofile = profilelist;
    while (thisprofile) {
      if (thisprofile->uri) {
	newprofile->uri = stringseg;
	strcpy(stringseg, thisprofile->uri);

	if (cbxml_include_isneeded(stringseg))
	  stringseg += cbxml_include_inplace(stringseg) + 2;
	else
	  stringseg += strlen(stringseg) + 2; 
      }
      if (thisprofile->piggyback) {
	newprofile->piggyback = stringseg;
	memcpy(stringseg, thisprofile->piggyback, thisprofile->piggyback_length);

	/* newprofile->piggyback_length = thisprofile->piggyback_length; */
	  
	if (thisprofile->encoding) {
	  piggy_len = base64_dsize(stringseg);
	  if (0 >= piggy_len) {
	    /* $@$@$@$ fix the fault number */
	    FAULT(session, 8, "chan0_msg_fmt: invalid base64 encoding");
	    goto faulted;
	  } else {
	    newprofile->piggyback_length = piggy_len;
	    base64_decode_into(session, stringseg, stringseg);
	    stringseg[piggy_len] = 0;
	  }
	} else {
	  if (cbxml_include_isneeded(stringseg))
	    stringseg += cbxml_include_inplace(stringseg) + 2;
	  else
	    stringseg += strlen(stringseg) + 2; 

	  newprofile->piggyback_length = strlen(newprofile->piggyback);
	}
      }

      thisprofile = thisprofile->next;
      if (thisprofile) {
	newprofile->next = (struct profile *)((char*)newprofile + sizeof(struct profile));
	newprofile = newprofile->next;
      } 
    }
  }

  /* is this a start or close indication, we expect a channel number */
  if ('i' == mychan0_msg.op_ic && 
      ('s' == mychan0_msg.op_sc || 'c' == mychan0_msg.op_sc)) {
    if (number_len) {
      thisframe->payload[number_offset + number_len] = '\0';
      mychan0_ptr->channel_number = atol(thisframe->payload + number_offset);
    }

    if (server_name_len) {
      mychan0_ptr->server_name = stringseg;
      strncpy(stringseg, thisframe->payload + server_name_offset, server_name_len);

      if (cbxml_include_isneeded(stringseg))
	stringseg += cbxml_include_inplace(stringseg) + 2;
      else
	stringseg += server_name_len + 2; 
    }
  }

  if (features_len) {
    mychan0_ptr->features = stringseg;
    strncpy(stringseg, thisframe->payload + features_offset, features_len);

    if (cbxml_include_isneeded(stringseg))
      stringseg += cbxml_include_inplace(stringseg) + 2;
    else
      stringseg += features_len + 2; 
  }

  if (localize_len) {
    mychan0_ptr->localize = stringseg;
    strncpy(stringseg, thisframe->payload + localize_offset, localize_len);

    if (cbxml_include_isneeded(stringseg))
      stringseg += cbxml_include_inplace(stringseg) + 2;
    else
      stringseg += localize_len + 2; 
  }

  goto cleanup;
 faulted:
  if (mychan0_ptr) 
    session->free(mychan0_ptr);

 cleanup:
  /* clean up */
  if (thisframe != chan0_frame) 
    blu_frame_destroy(thisframe);

  while (profilelist) {
    thisprofile = profilelist;
    profilelist = profilelist->next;
    session->free(thisprofile);
  }  
  
  return(mychan0_ptr);
}

/*  
 * chan0_msgfmt 
 *
 * Given a chan0_message struct it formats the proper frame payload
 * and populates the frame.
 *
 * Returns NULL if memory allocation fails.
 * Calls FAULT() and returns NULL if there is an error detected.
 */
struct frame *  chan0_msg_fmt (struct session * session, 
			       struct chan0_msg * chan0_msg) {

  int size;
  char * payload = NULL, frametype;
  struct frame * myframe = NULL;


  switch (chan0_msg->op_ic) {
  case 'i':
    frametype = 'M';
    switch (chan0_msg->op_sc) {
    case 's':
      payload = chan0_msg_fmt_start(session, chan0_msg);
      break;
    case 'c':
      payload = chan0_msg_fmt_close(session, chan0_msg);
      break;
    default:
      FAULT(session, 451, "chan0_msg_fmt: data error (op_sc)");
      goto cleanup;
      break;
    }
    break;
  case 'c':
    frametype = 'R';
    switch (chan0_msg->op_sc) {
    case 'g':
      payload = chan0_msg_fmt_greet(session, chan0_msg);
      break;
    case 's':
      if ((NULL == chan0_msg->profile) || (NULL != chan0_msg->profile->next)) {
        if (chan0_msg->error == NULL)
        {
	    FAULT(session, 451, "chan0_msg_fmt: start conf. takes one profile");
	    goto cleanup;
        }
        else
        {
            frametype = 'E';
            payload = chan0_msg_fmt_error(session, chan0_msg);
            break;
        }
      }
      payload = chan0_msg_fmt_profile(session, chan0_msg);
      break;
    case 'c':
      payload = chan0_msg_fmt_ok(session, chan0_msg);
      break;
    case 'e':
      frametype = 'E';
      payload = chan0_msg_fmt_error(session, chan0_msg);
      break;
    default:
      FAULT(session, 451, "chan0_msg_fmt: data error (op_sc)");
      goto cleanup;
      break;
    }
    break;
  default:
    FAULT(session, 451, "chan0_msg_fmt: data error (op_ic)");
    goto cleanup;
    break;
  }

  /* allocate and poulate the new frame */
  if (payload) {
    size = strlen(payload);

    if (NULL == (myframe = blu_frame_create(session, size + 1))) {
      FAULT(session, 451, "chan0_msg_fmt: allocate frame failed.");    
    } else {
      myframe->channel_number = 0;
      myframe->answer_number = 0;
      myframe->more = '.';

      myframe->message_number = chan0_msg->message_number;
      myframe->msg_type = frametype;
      myframe->size = size;
      strcpy(myframe->payload, payload);
    }
  }

 cleanup:
  if (payload) 
    session->free(payload);
  return(myframe);
}

/*  
 * chan0_msg_fmt_profile 
 *
 * Given a profile struct it formats the proper XML suitable for
 * a channel 0 profile message, the confirmation of a successfull
 * start message.  The XML is returned in new memory.
 *
 * Returns NULL if memory allocation fails.
 * Calls FAULT() and returns NULL if there is an error detected.
 */
char *  chan0_msg_fmt_profile (struct session * session, 
			       struct chan0_msg * msg) {
  struct profile * myprofile;
  char * xml = NULL, * tmp;
  char * b64tmp = NULL;
  long size_ph, size_pt, size_ae;
  int size = 0, xmltmpsize = 0;
  char * xmltmpbuf = NULL;

  size_ph = strlen(CHAN0_ELEM_FMT_PROFILE_HEAD) - 2; 
  size_pt = strlen(CHAN0_ELEM_FMT_PROFILE_TAIL) - 2; 
  size_ae = strlen(CHAN0_ELEM_FMT_ATTRIB_ENCODING); 
  
  /* determine the size of the XML and alloc the buffer */
  /* $@$@$@$ Does not handle encoding size yet */

  if (msg->op_ic == 'c' && msg->op_sc == 's')
      size += strlen(chan0_mime_header);
  for (myprofile = msg->profile; NULL != myprofile; 
       myprofile = myprofile->next) {

    thingie_nmlize_sizer(myprofile->uri, "", &xmltmpsize, &size);
    size += size_ph;
    /*
      x = cbxml_normalize_length(myprofile->uri);
      maxtmp = MAX(x, maxtmp);
      size += size_ph + x;
    */

    myprofile->encoding = (char)0;
    if (myprofile->piggyback_length) {
      size += size_pt;      
      if (chan0_base64_isneeded(myprofile)) {
	myprofile->encoding = 'Y';
	size += size_ae + base64_esize(myprofile->piggyback_length);
      } else {
	thingie_nmlize_sizer(myprofile->piggyback, "", &xmltmpsize, &size);
	/*
	  x = cbxml_normalize_length(myprofile->piggyback);
	  maxtmp = MAX(x, maxtmp);
	  size += size_pt + x;      
	*/
      }
    } else {
      size += 2;      
    }
  }

  if (NULL == (xml = session->malloc(size + 1)) ||
      NULL == (xmltmpbuf = session->malloc(xmltmpsize + 1))) {
    /* $@$@$@$ fault code */
    FAULT(session, -2, "chan0_msg_fmt: memory allocation failed");
    goto fault;
  }

  /* having the buffer we need now format the xml */

  tmp = xml;
  if (msg->op_ic == 'c' && msg->op_sc == 's')
      tmp += sprintf(tmp,"%s",chan0_mime_header);
  for (myprofile = msg->profile; NULL != myprofile; 
       myprofile = myprofile->next) {

    thingie_nmlize_sprintf(myprofile->uri, &tmp, &xmltmpbuf, 
			   CHAN0_ELEM_FMT_PROFILE_HEAD);
    /*
      strcpy(strtmp, myprofile->uri);
      x = cbxml_normalize_inplace(strtmp);
      tmp += sprintf(tmp, CHAN0_ELEM_FMT_PROFILE_HEAD, strtmp);
    */
	  
    if (myprofile->piggyback_length) {
      /* $@$@$@$ this is a hack for now.  needs proper handling */
      if (myprofile->encoding) {
	if ((b64tmp = base64_encode(session, myprofile->piggyback, 
				    myprofile->piggyback_length))) {
	  tmp += sprintf(tmp, CHAN0_ELEM_FMT_ATTRIB_ENCODING);
	  tmp += sprintf(tmp, CHAN0_ELEM_FMT_PROFILE_TAIL, b64tmp); 
	  session->free(b64tmp);
	  b64tmp = NULL;
	} else {
	  /* $@$@$@$ fault code */
	  FAULT(session, -2, "chan0_msg_fmt: memory allocation failed");
	  goto fault;	  
	}
      } else {	
	thingie_nmlize_sprintf(myprofile->piggyback, &tmp, &xmltmpbuf, 
			       CHAN0_ELEM_FMT_PROFILE_TAIL);
	/*
	  strncpy(strtmp, myprofile->piggyback, myprofile->piggyback_length);
	  x = cbxml_normalize_inplace(strtmp);
	  tmp += sprintf(tmp, CHAN0_ELEM_FMT_PROFILE_TAIL, strtmp); 
	*/
      }
    } else {
      tmp += sprintf(tmp, CHAN0_ELEM_FMT_UNARY_CLOSE); 
    }
  }

  goto cleanup;
 fault:
  if (xml) {
    session->free(xml);
    xml = NULL;
  }
 cleanup:
  if (xmltmpbuf) {
    session->free(xmltmpbuf);
    xmltmpbuf = NULL;
  }
  if (b64tmp) {
    session->free(b64tmp);
    b64tmp = NULL;
  }

  return(xml);
}

/*  
 * chan0_msg_fmt_greeting
 *
 * Given a chan0_msg struct it formats the proper XML suitable for
 * a channel 0 greeting message.  The XML is returned in new memory.
 *
 * Returns NULL if memory allocation fails.
 * Calls FAULT() and returns NULL if there is an error detected.
 */
char *  chan0_msg_fmt_greet (struct session * session, 
			     struct chan0_msg * msg) {

  char * xml = NULL, * profilexml = NULL, * tmp;
  int size = 0, xmltmpsize = 0;
  char * xmltmpbuf = NULL;

  
  if (msg->profile) {
    if (NULL == (profilexml = chan0_msg_fmt_profile(session, msg))) {
      return(NULL);
    }
  }
    
  /*
   * Rough validation and prep done, so go for it.
   */

  size  = strlen(CHAN0_ELEM_FMT_GREET_HEAD);
  if (msg->features)
    thingie_nmlize_sizer(msg->features, CHAN0_ELEM_FMT_ATTRIB_FEAT, 
			 &xmltmpsize, &size);
  /*    size += strlen(CHAN0_ELEM_FMT_ATTRIB_FEAT) + strlen(msg->features); */
  if (msg->localize)
    thingie_nmlize_sizer(msg->localize, CHAN0_ELEM_FMT_ATTRIB_LOCL, 
			 &xmltmpsize, &size);
  /*      size += strlen(CHAN0_ELEM_FMT_ATTRIB_LOCL) + strlen(msg->localize); */
  if (NULL != profilexml) {
    size += strlen(CHAN0_ELEM_FMT_GREET_TAIL) + strlen(profilexml);
  } else {
    size += strlen(CHAN0_ELEM_FMT_UNARY_CLOSE);
  }

  if (NULL == (xml = session->malloc(size + 1)) ||
      NULL == (xmltmpbuf = session->malloc(xmltmpsize + 1))) {
    /* $@$@$@$ fault code */
    FAULT(session, -2, "chan0_msg_fmt: memory allocation failed");
    goto fault;
  } else {
    tmp = xml;
    tmp += sprintf(tmp, CHAN0_ELEM_FMT_GREET_HEAD);
    if (msg->features) {
      thingie_nmlize_sprintf(msg->features, &tmp, &xmltmpbuf, 
			     CHAN0_ELEM_FMT_ATTRIB_FEAT);
      /*  tmp += sprintf(tmp, CHAN0_ELEM_FMT_ATTRIB_FEAT, msg->features); */
    }
    if (msg->localize) {
      thingie_nmlize_sprintf(msg->localize, &tmp, &xmltmpbuf, 
			     CHAN0_ELEM_FMT_ATTRIB_LOCL);
      /*  tmp += sprintf(tmp, CHAN0_ELEM_FMT_ATTRIB_LOCL, msg->localize); */
    }
    if (NULL != profilexml) {
      tmp += sprintf(tmp, CHAN0_ELEM_FMT_GREET_TAIL, profilexml);
    } else {
      tmp += sprintf(tmp, CHAN0_ELEM_FMT_UNARY_CLOSE);
    }
  }

  goto cleanup;
 fault:
  if (xml) {
    session->free(xml);
    xml = NULL;
  }
 cleanup:
  if (xmltmpbuf) {
    session->free(xmltmpbuf);
    xmltmpbuf = NULL;
  }
  if (profilexml) 
    session->free(profilexml);

  return(xml);
}

/*  
 * chan0_msg_fmt_start 
 *
 * Given a chan0_msg it formats the proper XML suitable for
 * a channel 0 start message.  The XML is returned in new memory.
 *
 *
 * Returns NULL if memory allocation fails.
 * Calls FAULT() and returns NULL if there is an error detected.
 */
char *  chan0_msg_fmt_start (struct session * session, 
			     struct chan0_msg * msg) {

  char * xml = NULL, * profilexml = NULL, * tmp, numberbuf[16];
  int size;
  int xmltmpsize = 0;
  char * xmltmpbuf = NULL;

  /* Rough error checking for chan0_msg validity */
  if (NULL == msg->profile) {
    FAULT(session, 451, "chan0_fmt: start message without profile.");
    return(NULL);
  }

  if (NULL == (profilexml = chan0_msg_fmt_profile(session, msg))) {
      return(NULL);
  }
    
  /*
   * Rough validation and prep done, so go for it.
   */
  sprintf(numberbuf, "%d", (int)msg->channel_number);

  size  = strlen(CHAN0_ELEM_FMT_START_HEAD) + strlen(numberbuf);
  size += strlen(chan0_mime_header);
  size += strlen(CHAN0_ELEM_FMT_START_TAIL) + strlen(profilexml);
  if (msg->server_name) {
    thingie_nmlize_sizer(msg->server_name, CHAN0_ELEM_FMT_ATTRIB_SERVER, 
			 &xmltmpsize, &size);
    /*
      x = cbxml_normalize_length(msg->server_name);
      xmltmpsize = MAX(xmltmpsize, x);
      size += strlen(CHAN0_ELEM_FMT_ATTRIB_SERVER) + x;
    */
  }

  if (NULL == (xml = session->malloc(size + 1)) ||
      NULL == (xmltmpbuf = session->malloc(xmltmpsize + 1))) {
    /* $@$@$@ fault code */
    FAULT(session, -2, "Memory allocation failed.");
  } else {
    tmp = xml;
    tmp += sprintf(tmp, "%s",chan0_mime_header);
    tmp += sprintf(tmp, CHAN0_ELEM_FMT_START_HEAD, numberbuf);
    if (msg->server_name) {
      thingie_nmlize_sprintf(msg->server_name, &tmp, &xmltmpbuf, 
			     CHAN0_ELEM_FMT_ATTRIB_SERVER);
      /*
      if (cbxml_normalize_isneeded(msg->server_name)) {
	cbxml_normalize_into(msg->server_name, xmltmpbuf);
	tmp += sprintf(tmp, CHAN0_ELEM_FMT_ATTRIB_SERVER, xmltmpbuf);
      } else {
	tmp += sprintf(tmp, CHAN0_ELEM_FMT_ATTRIB_SERVER, msg->server_name);
      }
      */
    }
    tmp += sprintf(tmp, CHAN0_ELEM_FMT_START_TAIL, profilexml);
  }

  if (xmltmpbuf)
    session->free(xmltmpbuf);
  if (profilexml)
    session->free(profilexml);
  return(xml);
}

/*  
 * chan0_msg_fmt_close 
 *
 * Given a chan0_msg it formats the proper XML suitable for
 * a channel 0 close message.  The XML is returned in new memory.
 *
 *
 * Returns NULL if memory allocation fails.
 * Calls FAULT() and returns NULL if there is an error detected.
 */
char *  chan0_msg_fmt_close (struct session * session, 
			     struct chan0_msg * msg) {
  char * xml = NULL, * tmp, numberbuf[16];
  int size;
  int xmltmpsize = 0;
  char * xmltmpbuf = NULL;

  if (msg->error->code < 100 || msg->error->code > 999) 
    return(NULL);

  sprintf(numberbuf, "%d", (int)msg->channel_number);

  size = strlen(CHAN0_ELEM_FMT_CLOSE_HEAD) + strlen(numberbuf);
  size += strlen(chan0_mime_header);

  if (msg->error->lang) {
    thingie_nmlize_sizer(msg->error->lang, CHAN0_ELEM_FMT_ATTRIB_LANG, 
			 &xmltmpsize, &size);
    /*
      x = cbxml_normalize_length(msg->error->lang);
      xmltmpsize = MAX(xmltmpsize, x);
      size += strlen(CHAN0_ELEM_FMT_ATTRIB_LANG) + x;
    */
  }
  if (msg->error->message) {

    thingie_nmlize_sizer(msg->error->message, CHAN0_ELEM_FMT_CLOSE_TAIL, 
			 &xmltmpsize, &size);
    /*
    x = cbxml_normalize_length(msg->error->message);
    xmltmpsize = MAX(xmltmpsize, x);
    size += strlen(CHAN0_ELEM_FMT_CLOSE_TAIL) + x;
    */
  } else {
    size += 2;
  }

  if (NULL == (xml = session->malloc(size + 1)) ||
      NULL == (xmltmpbuf = session->malloc(xmltmpsize + 1))) {
    /* $@$@$@$ fault code */
    FAULT(session, -2, "Memory allocation failed.");
  } else {    
    tmp = xml;
    tmp += sprintf(tmp, "%s",chan0_mime_header);
    tmp += sprintf(tmp, CHAN0_ELEM_FMT_CLOSE_HEAD,  
		   (int)msg->channel_number, (int)msg->error->code);

    if (msg->error->lang) {
      thingie_nmlize_sprintf(msg->error->lang, &tmp, &xmltmpbuf, 
				CHAN0_ELEM_FMT_ATTRIB_LANG);
      /*
      if (cbxml_normalize_isneeded(msg->error->lang)) {
	cbxml_normalize_into(msg->error->lang, xmltmpbuf);
	tmp += sprintf(tmp, CHAN0_ELEM_FMT_ATTRIB_LANG, xmltmpbuf);
      } else {
	tmp += sprintf(tmp, CHAN0_ELEM_FMT_ATTRIB_LANG, msg->error->lang);
      }
      */
    }

    if (msg->error->message) {
      thingie_nmlize_sprintf(msg->error->message, &tmp, &xmltmpbuf, 
			     CHAN0_ELEM_FMT_CLOSE_TAIL);
      /*  tmp += sprintf(tmp, CHAN0_ELEM_FMT_CLOSE_TAIL, msg->error->message); */
    } else {
      tmp += sprintf(tmp, CHAN0_ELEM_FMT_UNARY_CLOSE);
    }
  }

  if (xmltmpbuf)
    session->free(xmltmpbuf);

  return(xml);
}

void thingie_nmlize_sprintf(char *data, char**dest, char **tmpbuf, char *fmt) {
  if (cbxml_normalize_isneeded(data)) {
    cbxml_normalize_into(data, *tmpbuf);
    *dest += sprintf(*dest, fmt, *tmpbuf);
  } else {
    *dest += sprintf(*dest, fmt, data);
  }
}

void thingie_nmlize_sizer(char *data, char *fmt, int *max, int *size) {
  int x;

  x = cbxml_normalize_length(data);
  *max = MAX(*max, x);
  *size += strlen(fmt) + x;
}

/*  
 * chan0_msg_fmt_error 
 *
 * Given an error struct it formats the proper XML suitable for
 * a channel 0 message.  The XML is returned in new memory.
 *
 *
 * Returns NULL if memory allocation fails.
 * Calls FAULT() and returns NULL if there is an error detected.
 */

char *  chan0_msg_fmt_error (struct session * session, 
			     struct chan0_msg * msg) {
  char * xml = NULL, * tmp;
  int size;
  int xmltmpsize = 0;
  char * xmltmpbuf = NULL;

  if (msg->error->code < 100 || msg->error->code > 999) 
    return(NULL);

  size = strlen(CHAN0_ELEM_FMT_ERROR_HEAD) + 1;
  size += strlen(chan0_mime_header);
  if (msg->error->lang) {
    thingie_nmlize_sizer(msg->error->lang, CHAN0_ELEM_FMT_ATTRIB_LANG, 
			 &xmltmpsize, &size);
    /*  size += strlen(CHAN0_ELEM_FMT_ATTRIB_LANG) + strlen(msg->error->lang); */
  }
  if (msg->error->message) {
    thingie_nmlize_sizer(msg->error->message, CHAN0_ELEM_FMT_ERROR_TAIL, 
			 &xmltmpsize, &size);
    /*  size += strlen(CHAN0_ELEM_FMT_ERROR_TAIL) + strlen(msg->error->message); */
  } else {
    size += 2;
  }

  if (NULL != (xml = session->malloc(size)) ||
      NULL != (xmltmpbuf = session->malloc(xmltmpsize + 1))) {
    tmp = xml;
    tmp += sprintf(tmp, "%s",chan0_mime_header);
    tmp += sprintf(tmp, CHAN0_ELEM_FMT_ERROR_HEAD, msg->error->code);
    if (msg->error->lang) {
      thingie_nmlize_sprintf(msg->error->lang, &tmp, &xmltmpbuf, 
			     CHAN0_ELEM_FMT_ATTRIB_LANG);
      /*  tmp += sprintf(tmp, CHAN0_ELEM_FMT_ATTRIB_LANG, msg->error->lang); */
    }
    if (msg->error->message) {
      thingie_nmlize_sprintf(msg->error->message, &tmp, &xmltmpbuf, 
			     CHAN0_ELEM_FMT_ERROR_TAIL);
      /*  tmp += sprintf(tmp, CHAN0_ELEM_FMT_ERROR_TAIL, msg->error->message); */
    } else {
      tmp += sprintf(tmp, "/>");
    }
  } else {    
    /* $@$@$@ fault code */
    FAULT(session, -2, "Memory allocation failed.");
    goto fault;
  }

  goto cleanup;
 fault:
  if (xml) {
    session->free(xml);
    xml = NULL;
  }

 cleanup:
  if (xmltmpbuf) {
    session->free(xmltmpbuf);
    xmltmpbuf = NULL;
  }

  return(xml);
}

/*  
 * chan0_msg_fmt_ok
 *
 * Given an errorstruct it formats the proper XML suitable for
 * a channel 0 message.  The XML is returned in new memory.
 *
 *
 * Returns NULL if memory allocation fails.
 * Calls FAULT() and returns NULL if there is an error detected.
 */

char *  chan0_msg_fmt_ok (struct session * session, 
			  struct chan0_msg * msg) {
  char * xml = NULL,*tmp;
  long size;

  size = strlen(CHAN0_ELEM_FMT_OK) + 1;
  size += strlen(chan0_mime_header);

  if (NULL != (xml = session->malloc(size))) {
    tmp = xml;
    tmp += sprintf(tmp,"%s",chan0_mime_header);
    memcpy(tmp, CHAN0_ELEM_FMT_OK, (size-strlen(chan0_mime_header)));
    xml[size - 1] = 0;
  } else {    
      FAULT(session, 451, "Memory allocation failed.");
  }

  return(xml);
}

/*
 * chan0_base64_isneeded
 *
 * We need a rotuine to detect whether base64 enciding is required
 * for the piggyback data for a profile.  This will be true if the 
 * piggyback data contains a character not in the XML character set.
 *
 * $@$@$@$ NB we only support UTF 8 right now.
 */
int chan0_base64_isneeded(struct profile * profile) {
  char * pb;
  int x;

  while (profile) {
    if (profile->piggyback) {
      for (x = 0, pb = profile->piggyback; 
	   x < profile->piggyback_length; x++, pb++) {
	if (XML_NVLD & XML_UTF8_charset[(int)*pb])
	  return(1);
      }
    }
    profile = profile->next;
  }
  
  return(0);
}

/*
 * frame_bcopy
 *
 * Essentially a copy utility from frame payload to the 
 * destination.  Useful since the frame payload may be 
 * discontinuous.
 *
 * Returns the number of bytes copied
 */
int frame_bcopy (struct session * session, struct frame * inframe,
		 char * outbuf, long offset, long size) {

  long bytescopied = 0, thissize;
  struct frame * thisframe;
  char * payloadbuf;

  if (NULL == session || NULL == inframe) {
    return(0);
  }
  
  /* transfer the frame payload  */
  thisframe = inframe;

  do {
    payloadbuf = thisframe->payload;

    if (thisframe->size > offset) {
      payloadbuf += offset;
      thissize = MIN(thisframe->size - offset, size);
      memcpy(outbuf, payloadbuf, thissize);
      
      offset = 0;
      size -= thissize;
      payloadbuf += thissize;      
      bytescopied += thissize;      
    } else {
      offset -= thisframe->size;
    }

    thisframe = thisframe->next_in_message;
  } while (NULL != thisframe && 0 < size);

  /* finis */
  return(bytescopied);
}

