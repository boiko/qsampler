// parser.h
//
/****************************************************************************
   liblscp - LinuxSampler Control Protocol API
   Copyright (C) 2004, rncbc aka Rui Nuno Capela. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

#ifndef __LSCP_PARSER_H
#define __LSCP_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__WIN32__)
#define WIN32
#endif


#if defined(__cplusplus)
extern "C" {
#endif

// strtok_r - needed in win32 for parsing results.
#if defined(WIN32)
char *strtok_r (char *pchBuffer, const char *pszDelim, char **ppch);
#endif

//-------------------------------------------------------------------------
// Simple token parser.

typedef struct _lscp_parser_t
{
    char *pchBuffer;
    int   cchBuffer;
    char *pszToken;
    char *pch;

} lscp_parser_t;

void        lscp_parser_init    (lscp_parser_t *pParser, const char *pchBuffer, int cchBuffer);
const char *lscp_parser_next    (lscp_parser_t *pParser);
int         lscp_parser_nextint (lscp_parser_t *pParser);
float       lscp_parser_nextnum (lscp_parser_t *pParser);
int         lscp_parser_test    (lscp_parser_t *pParser, const char *pszToken);
int         lscp_parser_test2   (lscp_parser_t *pParser, const char *pszToken, const char *pszToken2);
void        lscp_parser_free    (lscp_parser_t *pParser);

#if defined(__cplusplus)
}
#endif

#endif // __LSCP_PARSER_H

// end of parser.h