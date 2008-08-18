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
 * xml_entities.c
 *
 * Routines for handling XML entities, knowing that we have 
 * malloc restrictions. 
 *
 */

char * __xml_parse_globals_c_ver__ = "$Id: xml_parse_globals.c,v 1.2 2001/10/30 06:00:40 brucem Exp $";

#include "xml_parse_constants.h"


/*
 *
 */

const long XML_UTF8_charset[] = {
  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,     /* 0x00 - 0x07 */
  XML_NVLD,  XML_SPAC,  XML_SPAC,  XML_NVLD,  XML_NVLD,  XML_SPAC,  XML_NVLD,  XML_NVLD,     /* 0x08 - 0x0f */
  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,     /* 0x10 - 0x17 */
  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,  XML_NVLD,     /* 0x18 - 0x1f */
  XML_SPAC,  XML_CHAR,  XML_QUOT,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_QUOT,     /* 0x20 - 0x27 */
  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_DASH,  XML_PERD,  XML_CHAR,     /* 0x28 - 0x2f */
  XML_NMBR,  XML_NMBR,  XML_NMBR,  XML_NMBR,  XML_NMBR,  XML_NMBR,  XML_NMBR,  XML_NMBR,     /* 0x30 - 0x37 */
  XML_NMBR,  XML_NMBR,  XML_COLN,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,     /* 0x38 - 0x3f */
  XML_CHAR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0x40 - 0x47 */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0x48 - 0x4f */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0x50 - 0x57 */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_USCR,     /* 0x58 - 0x5f */
  XML_CHAR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0x60 - 0x67 */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0x68 - 0x6f */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0x70 - 0x77 */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,     /* 0x78 - 0x7f */
  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,     /* 0x80 - 0x87 */
  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,     /* 0x88 - 0x8f */
  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,     /* 0x90 - 0x97 */
  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,     /* 0x98 - 0x9f */
  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,     /* 0xa0 - 0xa7 */
  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,     /* 0xa8 - 0xaf */
  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_XTDR,     /* 0xb0 - 0xb7 */
  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,  XML_CHAR,     /* 0xb8 - 0xbf */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0xc0 - 0xc7 */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0xc8 - 0xcf */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_CHAR,     /* 0xd0 - 0xd7 */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0xd8 - 0xdf */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0xe0 - 0xe7 */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0xe8 - 0xef */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_CHAR,     /* 0xf0 - 0xf7 */
  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,  XML_LTTR,     /* 0xf8 - 0xff */
};

