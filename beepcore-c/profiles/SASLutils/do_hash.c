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
 * This is the interface to the various hash routines.
 * It's separate, because we're using 3rd party code,
 * and all such code is baroque in different ways to
 * make it portable. This, however, exports just normal
 * C stuff.
 */

#include "global.h" /* For MD5 code */
#include "md5.h"
#include <stdlib.h>

void do_hash(int mechanism, char * source, int len, char dest[8]) {
    if (mechanism == 5) {
        MD5_CTX md5ctx;
        unsigned char md5out[16];
        int i;
        MD5Init(&md5ctx);
        MD5Update(&md5ctx, source, len);
        MD5Final(md5out, &md5ctx);
        for (i = 0; i < 8; i++)
            dest[i] = md5out[i] ^ md5out[i+8];
    } else {
        abort();
    }
}


