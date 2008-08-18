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
 * This is generic routines used by all SASL profiles.
 * These are not the routines for DB management.
 */

#include <string.h>
#include <assert.h>
#include <ctype.h>
#ifdef WIN32
#include <stdlib.h>
#endif
        /* No time to figure out cbeep-config wierdness */
#include "../../core/generic/base64.h"
#include "sasl_general.h"

/* Given a pointer to the payload of a frame or a piggyback string,
 * find the <blob>, parse out the base64 stuff, and decode the
 * base64. Store the data in parse_blob_data, and its length in
 * parse_blob_length. The input data should be \0 terminated.
 * Note this does a really sloppy job on error detection. Basically,
 * it looks for the string "blob", then a ">" after that, and 
 * parses all base-64 characters up to the next "<" sign.
 * It also does a trivial parse of the status='...' attribute
 * if it's there, and puts it in parse_blob_status.
 */

void parse_blob(char * data, struct sasl_data_block * sdb) {
    char * blobP;
    char b64[1000];
    int inx = 0;
    sdb->parse_blob_length = -1;
    sdb->parse_blob_status[0] = '\0';
    blobP = strstr(data, "blob");
    if (blobP == NULL) return; /* No blob */
    while (*blobP && *blobP != '>')
        blobP += 1;
    if (!*blobP) return; /* Bad syntax */
    sdb->parse_blob_length = 0;
    while (*blobP && *blobP != '<' && inx < sizeof(b64)-4) {
        if (   ('A' <= *blobP && *blobP <= 'Z') ||
               ('a' <= *blobP && *blobP <= 'z') ||
               ('0' <= *blobP && *blobP <= '9') ||
                '+' == *blobP ||
                '/' == *blobP ||
                '=' == *blobP ) {
           b64[inx++] = *blobP;
           b64[inx] = '\0';
        }
        blobP += 1;
    }
    if (inx != 0) {
        base64_decode_into(NULL, b64, sdb->parse_blob_data);
        sdb->parse_blob_length = base64_dsize(b64);
    }
    /* Now see if there's a status=... */
    blobP = strstr(data, "blob");
    while (*blobP && *blobP != '>' && 0 != strncmp(blobP, "status", 6))
        blobP += 1;
    if (*blobP == '\0' || *blobP == '>')
        return;
    while (*blobP && *blobP != '=')
        blobP += 1;
    while (*blobP && *blobP != '\'' && *blobP != '"')
        blobP += 1;
    if (*blobP) blobP += 1;
    inx = 0;
    while (*blobP && *blobP != '\'' && *blobP != '"' && 
            inx < sizeof(sdb->parse_blob_status) - 3) {
        sdb->parse_blob_status[inx++] = *blobP;
        blobP += 1;
        sdb->parse_blob_status[inx] = '\0';
    }
}

void to_hex(unsigned char hash[8], unsigned char hex[17]) {
    int inx;
    for (inx = 0; inx < 8; inx++) {
        sprintf(hex + 2 * inx, "%02x", hash[inx]);
    }
}

int from_hex(unsigned char * hex, int hex_len, unsigned char hash[8]) {
    int inx; int byte; int nybble; unsigned char c; unsigned hexval;
    inx = 0; byte = 0; nybble = 0;
    while (inx < hex_len && byte < 8) {
        while (inx < hex_len && isspace(hex[inx]))
            inx ++;
        if (hex_len <= inx) {
            return 0;
        }
        c = tolower(hex[inx]);
        if (!isxdigit(c)) {
            return 0;
        }
        if ('0' <= c && c <= '9') 
            hexval = c - '0';
        else
            hexval = c - 'a' + 10;
        if (nybble == 0)
            hash[byte] = hexval << 4;
        else
            hash[byte] += hexval;
        nybble += 1;
        if (nybble == 2) {
            byte += 1; nybble = 0;
        }
        inx += 1;
    }
    return 1;
}

/* 
 * This accepts the payload of an incoming frame, either a start payload or 
 * a frame. It checks to see if it's a valid anonymous login. If it
 * is, it returns a status complete and registers the name and mechanism.
 * If it isn't, it returns an appropriate error.
 */

int sasl_anonymous_server_guts (char *inframe,
                                struct sasl_data_block * sdb) {
    parse_blob (inframe, sdb);
    return SASL_COMPLETE;
}

/*
 * This takes trace information and formats a payload to login
 * anonymously.
 */

char *sasl_anonymous_client_guts (char *trace,
                                  struct sasl_data_block *sdb) {
    strcpy (sdb -> payload, "<blob>");
    if (trace)
        base64_encode_into (trace, strlen (trace),
                            sdb -> payload + strlen (sdb -> payload));
    strcat (sdb -> payload, "</blob>");

    return (sdb -> payload);
}


/*
 * This returns the payload of the first message from
 * client to server.
 */

char *sasl_otp_client_guts_1 (char *authenID,
                              char *authorID,
                              struct sasl_data_block *sdb) {
    unsigned char *up;
    unsigned char uncoded_payload[sizeof "<data> </data>" + (256 * 2)];

    if ((authenID == NULL)
            || strlen (authenID) > 255
            || ((authorID != NULL) && strlen (authorID) > 255))
        return NULL;

    up = uncoded_payload;
    if (authorID) {
        strcpy (uncoded_payload, authorID);
	up += strlen (up);
    }    
    *up++ = '\0';
    strcpy(up, authenID);
    up += strlen (up);

    strcpy (sdb -> payload, "<blob>");
    base64_encode_into (uncoded_payload, up - uncoded_payload,
                        sdb -> payload + strlen (sdb -> payload));
    strcat (sdb -> payload, "</blob>");

    return (sdb -> payload);
}


/*
 * This takes the payload with the <blob> that first arrives
 * from the client and returns what the client wants to know.
 */

int sasl_otp_server_guts_1(char * inframe, struct sasl_data_block * sdb) {
    int i, j;
    int locked;
    unsigned char seed[64];
    unsigned char mech[8];
    int count;
    unsigned char pass[8];
    unsigned char prompt[90];
    char prompt_encoded[180];

    parse_blob(inframe, sdb);
    if (sdb->parse_blob_length < 3)
        return SASL_ERROR_BLOB;
    for (i = j = 0; 
            i < sdb->parse_blob_length && i < 256 && 
            sdb->parse_blob_data[i]; j++, i++) 
        sdb->authorID[j] = sdb->parse_blob_data[i];
    sdb->authorID[j] = '\0';
    assert(strlen(sdb->authorID) < sizeof(sdb->authorID));
    if (i == sdb->parse_blob_length || i == 256)
        return SASL_ERROR_BLOB;
    i += 1;
    for (j = 0; 
            i < sdb->parse_blob_length && i < 256 && sdb->parse_blob_data[i]; 
            j++, i++) 
        sdb->authenID[j] = sdb->parse_blob_data[i];
    sdb->authenID[j] = '\0';
    assert(strlen(sdb->authenID) < sizeof(sdb->authenID));
    if (i != sdb->parse_blob_length)
        return SASL_ERROR_BLOB;
    if ((sdb->authorID[0]) && strcmp(sdb->authenID, sdb->authorID))
        return SASL_ERROR_PROXY;
    locked = sasl_otp_lock(sdb->path, sdb->authenID);
    if (locked == 1)
        return SASL_ERROR_USERNAME;
    else if (locked == 2)
        return SASL_ERROR_USERBUSY;
    i = sasl_otp_read(sdb->path, sdb->authenID, mech, &count, seed, pass);
    if (i != 0) {
        /* Couldn't read file, bad format, etc. */
        sasl_otp_unlock(sdb->path,sdb->authenID);
        return SASL_ERROR_USERNAME;
    }
    if (count <= 1) {
        sasl_otp_unlock(sdb->path,sdb->authenID);
        return SASL_ERROR_SEQUENCE;
    }
    sprintf(prompt, "otp-%s %d %s ext", mech, count-1, seed);
    base64_encode_into(prompt, strlen(prompt), prompt_encoded);
    sprintf(sdb->payload, "<blob>%s</blob>", prompt_encoded);
    return SASL_CONTINUE;
}

/*
 * This is the client side for answering the server's prompt.
 * Call it with the <blob> and the pass phrase from the user.
 */

char * sasl_otp_client_guts_2(char * inframe, char * user_pass, int *problem,
        struct sasl_data_block * sdb) {
    int mechanism;  /* 5 = md5, 1 = sha1 */
    int count;      /* how many times to hash */
    unsigned char seed[64];  /* Pointer to the seed string */
    int inx;        /* Pointer/counter */
    int seed_inx;   /* Pointer into seed aray */
    unsigned char to_hash[128];
    unsigned char hash[8];
    unsigned char to_encode[21];

    *problem = SASL_ERROR_BLOB;
    parse_blob(inframe, sdb);
    if (sdb->parse_blob_length < 8) return NULL;
    if (0 == strncmp(sdb->parse_blob_data, "otp-md5 ", 8))
        mechanism = 5;
//  else if (0 == strncmp(sdb->parse_blob_data, "otp-sha1 ", 9))
//      mechanism = 1;
    else {
        *problem = SASL_ERROR_ALGORITHM;
        return NULL;
    }
    for (inx = 6; !isspace(sdb->parse_blob_data[inx]); inx++)
        if (sdb->parse_blob_length <= inx) return NULL;
    while (isspace(sdb->parse_blob_data[inx])) 
        if (sdb->parse_blob_length <= inx) return NULL;
        else inx += 1;
    count = 0;
    while ('0' <= sdb->parse_blob_data[inx] && 
            sdb->parse_blob_data[inx] <= '9') {
        count = count * 10 + sdb->parse_blob_data[inx] - '0';
        if (sdb->parse_blob_length <= ++inx) return NULL;
    }
    if (!isspace(sdb->parse_blob_data[inx])) return NULL;
    while (isspace(sdb->parse_blob_data[inx])) 
        if (sdb->parse_blob_length <= inx) return NULL;
        else inx += 1;
    seed_inx = 0;
    while (!isspace(sdb->parse_blob_data[inx]) && 
            inx < sdb->parse_blob_length &&
            seed_inx < sizeof(seed)-1) {
        seed[seed_inx++] = sdb->parse_blob_data[inx++];
        seed[seed_inx] = '\0';
    }
    /* OK, now the string is parsed. */
    strcpy(to_hash, seed);
    strcat(to_hash, user_pass);
    /* printf("c2: to_hash='%s', count=%d, mechanism=%d\n", to_hash, count, mechanism); */
    do_hash(mechanism, to_hash, strlen(to_hash), hash);
    for (inx = 0; inx < count; inx++) {
        memmove(to_hash, hash, 8);
        do_hash(mechanism, to_hash, 8, hash);
    }
    /* Now we're hashed. Put it back. */
    strcpy(to_encode, "hex:");
    to_hex(hash, &to_encode[strlen(to_encode)]);
    strcpy(sdb->payload, "<blob>");
    base64_encode_into(to_encode, strlen(to_encode), 
            sdb->payload + strlen(sdb->payload));
    strcat(sdb->payload, "</blob>");
    return sdb->payload;
}

/*
 * Call this when the client responds to the prompt.
 */

int sasl_otp_server_guts_2(char * inframe, struct sasl_data_block * sdb) {
    int inx;
    unsigned char client[8];  /* What the client sent */
    unsigned char client_hashed[8]; /* One hash of 'client' */
    int i;
    int mechanism = -1;  /* 5 = md5, 1 = sha1 */
    unsigned char mech[8];
    int count;      /* how many times to hash (once) */
    unsigned char seed[64];  /* Pointer to the seed string */
    unsigned char filed[8];  /* What's in the file */
    /* unsigned char debug[18]; *//* for debugging output */

    parse_blob(inframe, sdb);
    if (0 == strcmp(sdb->parse_blob_status, "abort"))
        return SASL_ABORTED;
    if (0 != strncmp(sdb->parse_blob_data, "hex:", 4))
        return SASL_ERROR_HEXONLY;
    from_hex(sdb->parse_blob_data + 4, sdb->parse_blob_length - 4, client);

    i = sasl_otp_read(sdb->path, sdb->authenID, mech, &count, seed, filed);
    if (i != 0) {
        /* Couldn't read file, bad format, etc. */
        sasl_otp_unlock(sdb->path,sdb->authenID);
        return SASL_ERROR_USERNAME;
    }
    if (count <= 1) {
        sasl_otp_unlock(sdb->path,sdb->authenID);
        return SASL_ERROR_SEQUENCE;
    }
    if (0 == strcmp(mech, "md5"))
        mechanism = 5;
    else if (0 == strcmp(mech, "sha1"))
        mechanism = 1;
    count -= 1;
    do_hash(mechanism, client, 8, client_hashed);
    /* Here, pass should match what the client sent me. */
    /* to_hex(client_hashed, debug); printf("calced    ch=%s\n", debug); */
    /* to_hex(filed, debug);         printf("calced filed=%s\n", debug); */
    for (inx = 0; inx < 8; inx++) {
        if (filed[inx] != client_hashed[inx]) {
            sasl_otp_unlock(sdb->path,sdb->authenID);
            return SASL_ERROR_BADHASH;
        }
    }
    sasl_otp_write(sdb->path, sdb->authenID, 
            (mechanism == 5) ? "md5" : "sha1", 
            count, seed, client);
    sasl_otp_unlock(sdb->path,sdb->authenID);
    return SASL_COMPLETE;
}

/*
 * This creates the initial database file entry.
 */
void sasl_otp_initial(char *path, char authenID[64], char mech[8], int count, char seed[64], char pass[64]) {
    int mechanism;  /* 5 = md5, 1 = sha1 */
    int i;
    unsigned char hash[8];
    unsigned char temp[8];
    char phrase[130];

    if (0 != strcmp(mech, "md5")) {
        printf("Only md5 implemented now.\n");
        exit(1);
    } else {
        mechanism = 5;
    }
    strcpy(phrase, seed);
    strcat(phrase,pass);
    do_hash(mechanism, phrase, strlen(phrase), hash);
    /* to_hex(hash, phrase); printf("first hash=%s\n", phrase); */
    for (i = 0; i < count; i++) {
        memmove(temp, hash, 8);
        do_hash(mechanism, temp, 8, hash);
        /* to_hex(hash, phrase); printf("hash %d=%s\n", i, phrase); */
    }
    sasl_otp_write(path, authenID, 
            (mechanism == 5) ? "md5" : "sha1", 
            count, seed, hash);
    for (; i < count + 3; i++) {
        memmove(temp, hash, 8);
        do_hash(mechanism, temp, 8, hash);
        /* to_hex(hash, phrase); printf("hash %d=%s\n", i, phrase); */
    }
}



#if 0
static void test() {
    char * x;
    char * y;
    struct sasl_data_block sdb;
    struct sasl_data_block sdbC;
    struct sasl_data_block sdbS;

x = "content-type:application/beep+xml\r\n\r\n<start pro='...'><blob status='Yeppers!'>SGVsbG8=</blob>";

    parse_blob(x, &sdb);
    printf("parse_blob_data='%s'\n", sdb.parse_blob_data);
    printf("parse_blob_length='%d'\n", sdb.parse_blob_length);
    printf("parse_blob_status='%s'\n", sdb.parse_blob_status);

x = "content-type:application/beep+xml\r\n\r\n<start pro='...'><blob>SGVsbG8=</blob> status=\"whoops\"";
    parse_blob(x, &sdb);
    printf("parse_blob_data='%s'\n", sdb.parse_blob_data);
    printf("parse_blob_length='%d'\n", sdb.parse_blob_length);
    printf("parse_blob_status='%s'\n", sdb.parse_blob_status);

    x = "<blob>ZG5ld0BpbnZpc2libGUubmV0</blob>"; /* "dnew@invisible.net" */
    y = sasl_anonymous_server_guts(x, &sdb);
    printf("%s? %s!\n", sdb.parse_blob_data, y);

    x = "<blob>ZG5ldw==</blob>"; /* dnew */
    y = sasl_anonymous_server_guts(x, &sdb);
    printf("%s? %s!\n", sdb.parse_blob_data, y);

    x = "<blob>QGludmlzaWJsZS5uZXQ=</blob>"; /* @invisible.net */
    y = sasl_anonymous_server_guts(x, &sdb);
    printf("%s? %s!\n", sdb.parse_blob_data, y);

    x = "<blob>eEBpbnZpc2libGUubmV0</blob>"; /* x@invisible.net */
    y = sasl_anonymous_server_guts(x, &sdb);
    printf("%s? %s!\n", sdb.parse_blob_data, y);

    x = sasl_anonymous_client_guts_1("Hello@Example.Com", &sdbC);
    y = sasl_anonymous_server_guts(x, &sdbS);
    printf("%s? %s!\n", sdbS.parse_blob_data, y);

    printf("\n\n----------------\n\n");
    sasl_otp_initial("dnew@iw", "md5", 3, "seed42", "hello world");
    /* system("more /tmp/dnew@iw"); system("sleep 15"); */

    x = sasl_otp_client_guts_1("dnew@iw", &sdbC);
    parse_blob(x, &sdb);
    printf("client_1 = %s = %s\n", x, sdb.parse_blob_data);
    y = sasl_otp_server_guts_1(x, &sdbS);
    parse_blob(y, &sdb);
    printf("server_1 = %s = %s\n", y?y:"*NULL*", sdb.parse_blob_data);

    x = sasl_otp_client_guts_2(y, "hello world", &sdbC);
    if (x == NULL) {
      printf("client_2 returned null, %s\n", sdbC.parse_blob_data);
    } else {
      parse_blob(x, &sdb);
      printf("client_2 = %s = %s\n", x, sdb.parse_blob_data);
    }
    y = sasl_otp_server_guts_2(x, &sdbS);
    parse_blob(y, &sdb);
    printf("server_2 = %s = %s\n", y, sdb.parse_blob_data);


    { char abc[1000]; char pdq[1000];
      int i; int j;
      for (i = 1; i < 10; i++) {
        base64_encode_into("Hello@Example.Com", i, abc);
        j = base64_decode_into(NULL, abc, pdq); 
        if (j != i || 0 != strncmp("Hello@Example.Com", pdq, i))
          printf("%d %d %s %s\n", i, j, abc, pdq);
      }
    }

    /* OK.  A real base64 test. */
    { unsigned char abc[4]; unsigned char pdq[8]; unsigned char xyz[4];
      int i, j;
      for (i = 0; i < 256; i++) {
        abc[0] = i;
        base64_encode_into(abc, 1, pdq);
        j = base64_decode_into(NULL, pdq, xyz);
        if (j != 1 || xyz[0] != abc[0]) {
          printf("Fault 1: i=%d, j=%d, xyz[0] = %d, abc[0] = %d\n",
            i, j, xyz[0], abc[0]);
        }
      }
      for (i = 0; i < 65536; i++) {
        abc[0] = i % 256;
        abc[1] = i / 256;
        base64_encode_into(abc, 2, pdq);
        j = base64_decode_into(NULL, pdq, xyz);
        if (j != 2 || xyz[0] != abc[0] || xyz[1] != abc[1]) {
          printf("Fault 2: i=%d, j=%d, xyz[0] = %d, abc[0] = %d\n\txyz[1]=%d, abc[1]=%d\n",
            i, j, xyz[0], abc[0], xyz[1], abc[1]);
        }
      }
    }

    /* Now a to_hex from_hex test */
    { unsigned char hash[8]; unsigned char hex[17]; unsigned char unhash[8];
        int inx;
        for (inx = 0; inx < 8; inx++) {
            hash[inx] = ((inx + 5) << 4) + inx;
            /* printf("filling hash[%d]=%x\n", inx, hash[inx]); */
        }
        to_hex(hash, hex);
        if (0 != strcmp(hex, "5061728394a5b6c7")) {
            printf("to_hex returned %s\n", hex);
        }
        inx = from_hex(hex, strlen(hex), unhash);
        if (!inx) 
          printf("from_hex returned zero\n");
        for (inx = 0; inx < 8; inx++) {
            if (hash[inx] != unhash[inx]) {
                printf("hash[%d]=%x, unhash[%d]=%x\n", 
                        inx, hash[inx], inx, unhash[inx]);
            }
        }
    }

}

int main() {
    test();
    return 0;
}
#endif

