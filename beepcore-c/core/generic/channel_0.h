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
 * channel_0.h
 *
 * Funtionality specific to channel 0 processing, including 
 * parsing and formatting messages for channel 0.
 *
 */
#ifndef __CHANNEL_0_H__
#define __CHANNEL_0_H__

/*  const char * __channel_0_h_ver__ = "$ID: $"; */

#include "CBEEP.h"

/*
 * chan0_msg_parse
 *
 * Given a frame pointer to the head of a complete channel 0 beep
 * message, this routine parses that message and returns a populated
 * chan0_msg struct.
 *
 * Returns NULL if memory could not be allocated for the new chan0_msg
 *       or any part thereof.
 * Returns NULL and calls the internal error handler if any internal 
 *       error is detected or if there is any formatting or other
 *       error on the incoming frame (i.e. malformed xml and such).
 */
struct chan0_msg * chan0_msg_parse (struct session * session,
				    struct frame * chan0_frame);

/*
 * bll_chan0_msg_fmt
 *
 * Given a properly constructed and populated chan0_msg struc it will
 * generate a new frame struct populated with the XML payload to send.
 *
 * Returns NULL if memory could not be allocated.  
 * Calls the internal error handler and returns NULL if an invalid
 *       chan0_msg is passed in or if any of the underlying routines
 *       detected an error.
 */
struct frame *  chan0_msg_fmt (struct session * session, 
			       struct chan0_msg * chan0_msg);


/*
 * frame_bcopy: Comparable to bcopy, but understanding the non-
 * contiguous nature of the frame struct. Copies the indicated 
 * portion of the frame payload to the location pointed to by 
 * the outbuf argument.  Returns the number of bytes copied.
 * 
 */
int frame_bcopy (struct session * session, struct frame * inframe,
		 char * outbuf, long offset, long size);




/* __CHANNEL_0_H__ */
#endif
