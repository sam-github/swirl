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
#ifndef IO_STATE_H
#define IO_STATE_H 1

#include "../../core/generic/CBEEP.h"
#include "../../utility/bp_slist_utility.h"
#include "../../utility/bp_queue.h"
#include "../../utility/bp_hash.h"

#include "../utility/semutex.h"
#ifndef WIN32
#include <poll.h>
#endif

/* state 0 - new connection
   state 1 - ready for first read from from connection
   state 2 - want to send output to connection
   state 3 - normal operation
*/

#if defined(__cplusplus)
extern "C"
{
#endif

#define MAX_CONNECTIONS_PERTHREAD 1
#define DIO 2
#define QSIZE 32

typedef struct _io_state IO_STATE;

typedef struct
{
    sem_t lock;
    int poll_id;
    int shutdown;
    unsigned int size;
    unsigned int count;
    IO_STATE **iostateindex;
#ifdef WIN32
    DWORD tid;
    HANDLE threadHandle;
#else
    THREAD_T tid;
    struct pollfd *sessionfds;
#endif
} POLL_NODE;

/*
 * IO_STATE wraps each socket. There is a 1-1 mapping between IO_STATE
 * and WRAPPER structure.
 */

/*
 * IO_STATE::state
 *      0 connected
 *      1 the first write goes out (greeting), ready for first read
 *      2 not used (used to test SEQs go out in the right order)
 *      3 up and running
 *      4 while writing another write event happened (from notify_lower)
 */
#define IOS_STATE_SHUTDOWN -1
#define IOS_STATE_INITIALIZING -1
#define IOS_STATE_CONNECTED     0
#define IOS_STATE_GREETING_SENT 1
#define IOS_STATE_TEST_SEQ      2
#define IOS_STATE_ACTIVE        3
#define IOS_STATE_WRITE_PENDING 4

#define IOS_EVENT_READ  1
#define IOS_EVENT_WRITE 2

typedef struct _reader_writer READER_WRITER;
struct _reader_writer;

struct _io_state
{
    sem_t lock;
    int state;
    int readable;
    int shutdown;
    POLL_NODE *pn;
    unsigned int iostate_index;
    struct _wrapper* wrapper;
#ifdef WIN32
    /* read and write are enabled/disabled by setting the event flags */
    int events;
    SOCKET socket;
#else
    int socket;
#endif
    bp_slist *stack_reader_writer;
};
    
extern IO_STATE* fiostate_new(int fd);
extern void fiostate_delete(IO_STATE* ios);
extern void fiostate_set_wrapper(IO_STATE *ios,struct _wrapper *wrapper);

extern int fiostate_start(IO_STATE *ios);
extern void fiostate_stop(IO_STATE *ios);

extern void fiostate_block_write(IO_STATE *ios);
extern void fiostate_unblock_write(IO_STATE *ios);
extern void fiostate_block_read(IO_STATE *ios);
extern void fiostate_unblock_read(IO_STATE *ios);
extern int IW_fpoll_max(int max_files);

typedef int (*RWIO)(void *, char *,int);
typedef int (*FILTERIO)(void *,char *,char **,int,int *);
typedef int (*IODESTROY)(void *);

/**
 * Pushes a read/write function to the I/O stack for the connection.
 *
 * <p>
 * Only the top r/w functions get called.  To push a filter, use
 * {@link bp_push_rwfilter bp_push_rwfilter}.
 *
 * @param ios          A pointer to the IOSTATE structure.
 *
 * @param reader       Function pointer for read filter.
 *
 * @param readhandle   User-supplied pointer for the reader function.
 *
 * @param writer       Function pointer for write filter.
 *
 * @param writehandle  User-supplied pointer for the writer function.
 *
 * @param finreader    Function pointer for releasing any reader-specific
 *                     resources.
 *
 * @param finwriter    Function pointer for releasing any writer-specific
 *                     resources.
 *
 * @return <b>BLW_OK</b> or <b>BLW_ERROR</b>.
 **/
extern int bp_new_reader_writer(IO_STATE* ios,
                                RWIO reader,
                                void *readhandle,
                                RWIO writer,
                                void *writehandle,
                                int readsize,
                                IODESTROY finreader,
                                IODESTROY finwriter);

/**
 * Adds a read/write function to the I/O filter stack for the connection.
 *
 * <p>
 * Multiple simultaneous tuning profiles are supported, as each
 * operates as a filter or pump in the pipleline from the raw descriptor
 * to what is actually passed into the BEEP core.
 *
 * @param ios          A pointer to the IO_STATE structure.
 *
 * @param reader       Function pointer for the read filter.
 *
 * @param readhandle   User-supplied pointer for the read filter.
 *
 * @param writer       Function pointer for the write filter.
 *
 * @param writehandle  User-supplied pointer for the write filter.
 *
 * @param rdestroy    Function pointer for releasing any reader-specific
 *                     resources.
 *
 * @param wdestroy    Function pointer for releasing any writer-specific
 *                     resources.
 *
 * @return <b>BLW_OK</b> or <b>BLW_ERROR</b>.
 **/
extern int bp_push_rwfilter(IO_STATE* ios,
                            FILTERIO reader,
                            void *readhandle,
                            FILTERIO writer,
                            void *writehandle,
                            IODESTROY rdestroy,
                            IODESTROY wdestroy);

#if defined(__cplusplus)
}
#endif

#endif    
