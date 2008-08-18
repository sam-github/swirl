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

struct sasl_data_block {
    char *path;
    unsigned char parse_blob_data[1000];
    int parse_blob_length; /* gets <0 for problems */
    char parse_blob_status[25];
    char payload[800];  /* Return value */
    char authenID[260];
    char authorID[260];
    int locked;
};

#define SASL_ALREADY_DONE     (-3)
#define SASL_ABORTED          (-2)
#define SASL_CONTINUE         (-1)
#define SASL_COMPLETE           0
#define SASL_ERROR_BLOB         1
#define SASL_ERROR_TRACE        2
#define SASL_ERROR_PROXY        3
#define SASL_ERROR_USERNAME     4
#define SASL_ERROR_USERBUSY     5
#define SASL_ERROR_SEQUENCE     6
#define SASL_ERROR_HEXONLY      7
#define SASL_ERROR_BADHASH      8
#define SASL_ERROR_ALGORITHM    9

int sasl_anonymous_server_guts (char *inframe, struct sasl_data_block *sdb);
int sasl_otp_server_guts_1     (char *inframe, struct sasl_data_block *sdb);
int sasl_otp_server_guts_2     (char *inframe, struct sasl_data_block *sdb);


char *sasl_anonymous_client_guts (char *trace, struct sasl_data_block *sdb);
char *sasl_otp_client_guts_1     (char *authenID, char *authorID,
                                               struct sasl_data_block *sdb);
char *sasl_otp_client_guts_2     (char *nframe, char *user_pass, int *problem,
                                               struct sasl_data_block *sdb);


void parse_blob(char * data, struct sasl_data_block * sdb);

void to_hex(unsigned char hash[8], unsigned char hex[17]);

int from_hex(unsigned char * hex, int hex_len, unsigned char hash[8]);

void sasl_otp_initial(char *path, char authenID[64], char mech[8], int count, char seed[64], char pass[64]);

int sasl_otp_lock(char *path,
        char * authID);

void sasl_otp_unlock(char *path,
        char * authID);

int sasl_otp_read(char *path,
        char * authID,
        char mech[8],
        int * count,
        char seed[64],
        char pass[8]);

int sasl_otp_write(char *path,
        char * authID,
        char mech[8],
        int count,
        char seed[64],
        char pass[8]);

void do_hash(int mechanism, char * source, int len, char dest[8]);
