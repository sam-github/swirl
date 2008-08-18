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
 * OTP DB interface routines.
 * These interface to wherever you're storing your passwords.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif
#include "sasl_general.h"

/*
 * This locks a OTP password. The password to lock
 * belongs to the indicated authentication ID. The
 * return is 0 if all is well, 1 if no such user
 * is in the database, or 2 if the file was busy.
 *
 * The current implementation uses a separate file
 * and a separate .lock file for each user.
 *
 * A real implementation would break expired locks,
 * etc.
 */

#ifdef WIN32
int sasl_otp_lock(char *path,
        char * authID) {
    HANDLE handle;
    char fname[290];

    sprintf(fname, "%s/%s.LOCK", path, authID);
    handle = CreateFile(fname, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
	return 2;
    }
    CloseHandle(handle);

    handle = CreateFile(authID, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        DeleteFile(fname);
        return -2;
    }
    CloseHandle(handle);
    return 0;
}
#else
int sasl_otp_lock(char *path,
        char * authID) {
    int handle;
    char fname[290];

    sprintf(fname, "%s/%s.LOCK", path, authID);
    handle = open(fname, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (handle == -1) return 2;
    close(handle);
    handle = open(authID, O_RDWR);
    if (handle == -1) {
        unlink(fname);
        return -2;
    }
    close(handle);
    return 0;
}
#endif

/*
 * This unlocks a previously locked user name.
 * It does nothing if the name isn't locked.
 */

void sasl_otp_unlock(char *path,
        char * authID) {
    char fname[290];

    sprintf(fname, "%s/%s.LOCK", path, authID);
    unlink(fname);
}

/*
 * This reads an OTP record from the password database.
 * It returns 0 if all was well, else 1 for any problem.
 */

int sasl_otp_read(char *path,
        char * authID,
        char mech[8],
        int * count,
        char seed[64],
        char pass[8]) {
    FILE * pwfile;
    char count_str[20];
    char pass_encoded[18];
    char fname[290];

    sprintf(fname, "%s/%s", path, authID);
    pwfile = fopen(fname, "r");
    if (pwfile == NULL) return 1;
    if (NULL == fgets(mech, 8, pwfile)) {
        fclose(pwfile);
        return 1;
    }
    if (mech[strlen(mech)-1] == '\n')
        mech[strlen(mech)-1] = '\0';
    if (NULL == fgets(count_str, 20, pwfile)) {
        fclose(pwfile);
        return 1;
    }
    (*count) = atoi(count_str);
    if (NULL == fgets(seed, 64, pwfile)) {
        fclose(pwfile);
        return 1;
    }
    if (seed[strlen(seed)-1] == '\n')
        seed[strlen(seed)-1] = '\0';
    if (NULL == fgets(pass_encoded, 17, pwfile)) {
        fclose(pwfile);
        return 1;
    }
    from_hex(pass_encoded, 16, pass);
    fclose(pwfile);
    return 0;
}

/*
 * This writes an OTP record to the password database.
 * It returns 0 if all was well, else 1 for any problem.
 */

int sasl_otp_write(char *path,
        char * authID,
        char mech[8],
        int count,
        char seed[64],
        char pass[8]) {
    FILE * pwfile;
    char pass_encoded[18];
    char fname[290];

    sprintf(fname, "%s/%s", path, authID);
    pwfile = fopen(fname, "w");
    if (pwfile == NULL) return 1;
    to_hex(pass, pass_encoded);
    fprintf(pwfile, "%s\n%d\n%s\n%s\n", mech, count, seed, pass_encoded);
    fclose(pwfile);
    return 0;
}

