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
 * $Id: xml_parse_constants.h,v 1.2 2001/10/30 06:00:40 brucem Exp $
 *
 * xml_parse_constants.h
 *
 * Containing various static things for use in XML parsing.
 *
 */

/* Array of indicators for XML parsingm validity for spefic sates */
#define XML_CHAR               0x0001
#define XML_S                  0x0002
#define XML_LETTER             0x0004 
#define XML_NUMBER             0x0008

#define XML_NAME               0x0010
#define XML_NAMESTART          0x0020
#define XML_EXTENDER           0x0040
#define XML_QUOTE              0x0080

#define XML_ENTITYSTART        0x0100
#define XML_ENTITYCLOSE        0x0200
#define XML_NAMETOKEN          0x0400
/*
#define XML_CHAR               0x0800

#define XML_CHAR               0x1000
#define XML_CHAR               0x2000
#define XML_CHAR               0x4000
*/
#define XML_NVLD               0x8000

#define XML_SPAC               (XML_CHAR | XML_S)
#define XML_NMBR               (XML_CHAR | XML_NUMBER | XML_NAMESTART | XML_NAME | XML_NAMETOKEN)
#define XML_LTTR               (XML_CHAR | XML_LETTER | XML_NAMESTART | XML_NAME | XML_NAMETOKEN)
#define XML_USCR               (XML_CHAR | XML_NAMESTART | XML_NAME | XML_NAMETOKEN)
#define XML_COLN               (XML_CHAR | XML_NAMESTART | XML_NAME | XML_NAMETOKEN)
#define XML_PERD               (XML_CHAR | XML_NAME | XML_NAMETOKEN)
#define XML_DASH               (XML_CHAR | XML_NAME | XML_NAMETOKEN)
#define XML_QUOT               (XML_CHAR | XML_QUOTE)
#define XML_XTDR               (XML_CHAR | XML_EXTENDER)

extern const long XML_UTF8_charset[];
