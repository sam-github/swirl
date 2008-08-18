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

#ifndef WIN32
#include <stdlib.h>
#else
#include <windows.h>
#endif
#ifdef LINUX
#include <sys/socket.h>
#endif
#include "bp_fiostate.h"
#include "../utility/semutex.h"
#include "../../utility/bp_slist_utility.h"
#include "../wrapper/bp_wrapper.h"
#include "../wrapper/bp_wrapper_private.h"
#include "../utility/workthread.h"
#include "../utility/logutil.h"
#include "../../core/generic/CBEEPint.h"

extern int poll_node_add_iostate(IO_STATE* ios);

#define QSIZE 32

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !(FALSE)
#endif


/*
 * local struct definitions
 */

struct _reader_writer
{
    RWIO reader;
    RWIO writer;
    void *readhandle;
    void *writehandle;
    char *read_block;
    int read_len;
    int read_size;
    char *filter_buff;
    int filter_buff_len;
    IODESTROY cb_destroy_reader;
    IODESTROY cb_destroy_writer;
    bp_slist *read_filters;
    bp_slist *write_filters;
} _reader_writer;

struct _filter_func
{
    FILTERIO filter_func;
    void *filterhandle;
    IODESTROY filter_destroy;
} _filter_func;    

typedef struct _filter_func FILTER_FUNC;

/* reader writer stuff */

bp_slist *blw_pop_rwfilter(bp_slist *in);
bp_slist *blw_pop_reader_writer(IO_STATE *wrap);

int blw_reader(READER_WRITER *rwio, char *buff, int bytes_requested);
int blw_writer(READER_WRITER *rwio, char *buff, int bytes_requested);

int blw_primary_reader(void *fd, char *buff, int bytes);
int blw_primary_writer(void *fd, char *buff, int bytes);




IO_STATE* fiostate_new_ex(int fd, READER_WRITER *reader_writer)
{
   IO_STATE* ios;

   ios = (IO_STATE *)malloc(sizeof(IO_STATE));
   if (ios)
   {
      SEM_INIT(&ios->lock, 1);
      ios->socket = fd;
      ios->state = IOS_STATE_INITIALIZING;
      ios->readable = 0;
      ios->shutdown = 0;
#ifdef WIN32
      ios->events = IOS_EVENT_READ;
#endif
      ios->wrapper = NULL;
      ios->pn = NULL;
      ios->iostate_index = -1;
      ios->stack_reader_writer= NULL;
      if (!reader_writer) {
          bp_new_reader_writer(ios,
                               blw_primary_reader, &ios->socket,
                               blw_primary_writer, &ios->socket,
                               2048, NULL, NULL);
      } else {
          ios->stack_reader_writer =
              bp_slist__prepend(ios->stack_reader_writer,
                                reader_writer);
      }
   }
   return ios;
}

IO_STATE* fiostate_new(int fd)
{
    return fiostate_new_ex(fd, NULL);
}

void fiostate_delete(IO_STATE* ios)
{
    bp_slist * tmplist;

    ASSERT(ios->state == IOS_STATE_INITIALIZING ||
           ios->state == IOS_STATE_SHUTDOWN);

    log_line(LOG_WRAP, 0, "deleting iostate: %d", ios->socket);

    tmplist = ios->stack_reader_writer;
    while (tmplist)
        tmplist = blw_pop_reader_writer(ios);
    
    free(ios);
}

void fiostate_set_wrapper(IO_STATE* ios, struct _wrapper* wrapper)
{
    SEM_WAIT(&ios->lock);
    ios->wrapper = wrapper;
    SEM_POST(&ios->lock);
}

int fiostate_start(IO_STATE *ios)
{
    int err;

    err = poll_node_add_iostate(ios);
    if (err != 0) {
        return -1;
    }

    ios->state = IOS_STATE_CONNECTED;
    fiostate_unblock_read(ios);
    fiostate_unblock_write(ios);
    return 0;
}

void fiostate_stop(IO_STATE *ios)
{
    log_line(LOG_WRAP, 0, "stopping iostate: %d", ios->socket);
    ios->state = IOS_STATE_SHUTDOWN;
    ios->shutdown=1;
}

void fiostate_block_read(IO_STATE *ios)
{
#ifdef WIN32
    SEM_WAIT(&ios->lock);
    ios->events ^= IOS_EVENT_READ;
    SEM_POST(&ios->lock);
#else
    SEM_WAIT(&ios->pn->lock);
    if ((ios->pn->sessionfds[ios->iostate_index].events & (POLLPRI|POLLIN)) ==
        (POLLPRI|POLLIN))
    {
        ios->pn->sessionfds[ios->iostate_index].events ^= POLLPRI|POLLIN;
    }
    SEM_POST(&ios->pn->lock);
#endif
}

void fiostate_unblock_read(IO_STATE *ios)
{
#ifdef WIN32
    SEM_WAIT(&ios->lock);
    ios->events |= IOS_EVENT_READ;
    SEM_POST(&ios->lock);
#else
    SEM_WAIT(&ios->pn->lock);
    ios->pn->sessionfds[ios->iostate_index].events |= POLLPRI|POLLIN;
    SEM_POST(&ios->pn->lock);
#endif
}  

void fiostate_block_write(IO_STATE *ios)
{
#ifdef WIN32
    SEM_WAIT(&ios->lock);
    ios->events ^= IOS_EVENT_WRITE;
    SEM_POST(&ios->lock);
#else
    SEM_WAIT(&ios->pn->lock);
    if ((ios->pn->sessionfds[ios->iostate_index].events & POLLOUT) == POLLOUT)
        ios->pn->sessionfds[ios->iostate_index].events ^= POLLOUT;
    SEM_POST(&ios->pn->lock);
#endif
}

void fiostate_unblock_write(IO_STATE *ios)
{
#ifdef WIN32
    SEM_WAIT(&ios->lock);
    ios->events |= IOS_EVENT_WRITE;
    SEM_POST(&ios->lock);
#else
    SEM_WAIT(&ios->pn->lock);
    ios->pn->sessionfds[ios->iostate_index].events |= POLLOUT;
    SEM_POST(&ios->pn->lock);
#endif
} 

int bp_new_reader_writer(IO_STATE* ios, RWIO reader, void* readhandle,
                         RWIO writer, void* writehandle, int readsize,
                         IODESTROY finreader, IODESTROY finwriter)
{
    bp_slist *stack_of_rw;
    READER_WRITER *rwio=NULL;

    stack_of_rw = ios->stack_reader_writer;
    rwio = (READER_WRITER *)lib_malloc(sizeof(READER_WRITER));
    if (!rwio)
       return BLW_ERROR;
    rwio->reader=reader;
    rwio->writer=writer;
    rwio->readhandle = readhandle;
    rwio->writehandle = writehandle;
    if (readsize < 1024)
        readsize = 1024;
    rwio->read_block = (char *)lib_malloc(readsize);
    memset(rwio->read_block,'\0',readsize);
    rwio->read_size = readsize;
    rwio->read_len = 0;
    rwio->filter_buff = NULL;
    rwio->filter_buff_len = 0;
    rwio->read_filters = NULL;
    rwio->write_filters = NULL;
    rwio->cb_destroy_reader = finreader;
    rwio->cb_destroy_writer = finwriter;
    ios->stack_reader_writer = bp_slist__prepend(ios->stack_reader_writer, rwio);
    return BLW_OK;
}

/*
READER_WRITER *blw_set_reader_writer(RWIO reader,void *readhandle,RWIO writer
,void *writehandle,int readsize,IODESTROY finreader,IODESTROY finwriter)
{
    READER_WRITER *rwio=NULL;

    rwio = (READER_WRITER *)lib_malloc(sizeof(READER_WRITER));
    if (!rwio)
       return NULL;
    rwio->reader=reader;
    rwio->writer=writer;
    rwio->readhandle = readhandle;
    rwio->writehandle = writehandle;
    if (readsize < 1024)
        readsize = 1024;
    rwio->read_block = (char *)lib_malloc(readsize);
    memset(rwio->read_block,'\0',readsize);
    rwio->read_size = readsize;
    rwio->read_len = 0;
    rwio->filter_buff = NULL;
    rwio->filter_buff_len = 0;
    rwio->read_filters = NULL;
    rwio->write_filters = NULL;
    rwio->cb_destroy_reader = finreader;
    rwio->cb_destroy_writer = finwriter;
    return rwio;
}
*/

READER_WRITER *blw_get_reader_writer(IO_STATE* ios)
{
    READER_WRITER *ret;

    ret = (READER_WRITER *)ios->stack_reader_writer->data;
    return ret;
}

bp_slist *blw_pop_reader_writer(IO_STATE* ios)
{

    bp_slist *tmp;
    READER_WRITER *fin;

    fin = (READER_WRITER *)ios->stack_reader_writer->data;
    if (!fin)
        return 0;
    if (fin->cb_destroy_reader)
        fin->cb_destroy_reader(fin->readhandle);
    if (fin->cb_destroy_writer)
        fin->cb_destroy_writer(fin->writehandle);
    tmp = fin->read_filters;
    while (tmp)
        tmp = blw_pop_rwfilter(tmp);
    tmp = fin->write_filters;
    while (tmp)
        tmp = blw_pop_rwfilter(tmp);
    lib_free(fin->read_block);
    lib_free(fin->filter_buff);
    lib_free(fin);
    ios->stack_reader_writer = bp_slist__remove_index(ios->stack_reader_writer,0);
    return ios->stack_reader_writer;
}

int bp_push_rwfilter(IO_STATE* ios,
                     FILTERIO rfilter, void *rhandle,
                     FILTERIO wfilter, void *whandle,
                     IODESTROY rdestroy, IODESTROY wdestroy)
{
    READER_WRITER *rw = (READER_WRITER *) ios -> stack_reader_writer -> data;
    FILTER_FUNC *rf,
        *wf;

    if (!(rf = lib_malloc (sizeof *rf)) || !(wf = lib_malloc (sizeof *wf))) {
        if (rf)
            lib_free (rf);
        return BLW_ERROR;
    }

    rf -> filter_func = rfilter;
    rf -> filterhandle = rhandle;
    rf -> filter_destroy = rdestroy;
    rw -> read_filters = bp_slist__append (rw -> read_filters, rf);

    wf -> filter_func = wfilter;
    wf -> filterhandle = whandle;
    wf -> filter_destroy = wdestroy;
    rw -> write_filters = bp_slist__prepend (rw -> write_filters, wf);

    return BLW_OK;
}


bp_slist *blw_pop_rwfilter(bp_slist *in)
{
    bp_slist *tmp;
    FILTER_FUNC *filter;

    if (!in)
        return NULL;
    if (!in->data)
        return NULL;
    filter = (FILTER_FUNC *)in->data;
    if (filter->filter_destroy)
        filter->filter_destroy(filter->filterhandle);
    lib_free(filter);
    tmp = bp_slist__remove_index(in,0);
    return tmp;
}

int blw_writer(READER_WRITER *rwio,char *buff,int bytes_requested)
{
    int i,ilen,inlen,ifirst,filterbytes,byteswrote,inbytesused;
    FILTER_FUNC *filter_node;
    bp_slist *filterlist;
    char *inbuf=NULL,*outbuf=NULL;

    if (rwio == NULL)
        return 0;

    inlen = bytes_requested;
    inbuf = buff;
    ifirst = 1;
    filterbytes = bytes_requested;

    if (rwio->write_filters)
    {
        ilen = bp_slist__length(rwio->write_filters);
        for (i=0;i<ilen;i++)
        {
            filterlist = bp_slist__index(rwio->write_filters,i);
            filter_node = (FILTER_FUNC *)filterlist->data;
            if (filter_node)
            {
                if (filter_node->filter_func)
                {
                    filterbytes = filter_node->filter_func(filter_node->filterhandle,
                                                           inbuf,
                                                           &outbuf,
                                                           inlen,
                                                           &inbytesused);
                    if (filterbytes <= 0)
                        return filterbytes;
                    if (!ifirst)
                    {
                        lib_free(inbuf);
                        ifirst = 0;
                    }
                    inbuf = outbuf;
                 }
             }
        }
    }
    byteswrote = rwio->writer(rwio->writehandle,inbuf,filterbytes);
    return byteswrote;
}



int blw_reader(READER_WRITER *rwio,char *buff,int bytes_requested)
{
    int iKnt,bytesread = 0;
    int i,ilen,ifirst=0,iblockend=0;
    int filterbytes,save_read_len,inlen,inbytesused,inbytes;
    bp_slist *filterlist;
    char *inbuf=NULL,*outbuf=NULL,*save_read_buff = NULL;
    FILTER_FUNC *filter_node;

    if (rwio == NULL)
        return 0;

    do
    {
        if (rwio->filter_buff)
        {
            if (rwio->filter_buff_len >= bytes_requested)
            {
                iKnt = bytes_requested;
                while (iKnt != 0 && (rwio->filter_buff[iKnt-1] != '\n'))
                    iKnt--;
                if (iKnt == 0)
                    iKnt = bytes_requested;
                memcpy(buff,rwio->filter_buff,iKnt);
                if (rwio->filter_buff_len != iKnt)
                {
                    rwio->filter_buff_len -= iKnt;
                    memcpy(rwio->filter_buff,rwio->filter_buff+iKnt,
                           rwio->filter_buff_len);
                } 
                else
                {
                    lib_free(rwio->filter_buff);
                    rwio->filter_buff = NULL;
                    rwio->filter_buff_len = 0;
                }
/*                printf("1 start %d %d %d %s 1 end\n",bytes_requested,iKnt,rwio->filter_buff_len,buff);
 */
                return iKnt;
            }
            else
            {
                if (iblockend)
                {
                    iKnt = (-1);
                    if (rwio->filter_buff_len)
                    {
                        if (rwio->filter_buff_len >= bytes_requested)
                        {
                            iKnt = bytes_requested;
                            while (iKnt != 0 && (rwio->filter_buff[iKnt-1] != '\n'))
                                iKnt--;
                            if (iKnt == 0)
                                iKnt = bytes_requested;
                            memcpy(buff,rwio->filter_buff,iKnt);
                            if (rwio->filter_buff_len != iKnt)
                            {
                                rwio->filter_buff_len -= iKnt;
                                memcpy(rwio->filter_buff,rwio->filter_buff+iKnt,
                                rwio->filter_buff_len);
                            }
                            else
                            {
                                lib_free(rwio->filter_buff);
                                rwio->filter_buff = NULL;
                                rwio->filter_buff_len = 0;
                            }
                        }
                        else
                        {
                            iKnt = rwio->filter_buff_len;
                            
                            memcpy(buff,rwio->filter_buff,rwio->filter_buff_len);
                            lib_free(rwio->filter_buff);
                            rwio->filter_buff = NULL;
                            rwio->filter_buff_len = 0;
                        }
                    }
/*
                    printf("2 start %d %d %s 2 end\n",bytes_requested,iKnt,buff);
*/
                    return iKnt;
                }
            }            
        }
        if (iblockend)
        {
/*
            printf("3 start %d %s 3 end\n",bytesread,buff);
*/
            return bytesread;
        }           

readmore:
 
        if ((rwio->read_size-rwio->read_len) == 0)
        {
            outbuf = (char *)lib_malloc(rwio->read_size+512);
            if (outbuf)
            {
                memcpy(outbuf,rwio->read_block,rwio->read_size);
                lib_free(rwio->read_block);
                rwio->read_block = outbuf;
                outbuf = 0;
                rwio->read_size += 512;
            }
        }

        bytesread = rwio->reader(rwio->readhandle,rwio->read_block+rwio->read_len,rwio->read_size-rwio->read_len);
        if (bytesread <= 0)
        {
            iblockend=1;
            continue;
        }

        rwio->read_len += bytesread;
        ifirst = 1; 
        filterbytes = bytesread;
        inbuf = rwio->read_block;
        save_read_len = 0;
        if (rwio->read_filters)
        {
            ilen = bp_slist__length(rwio->read_filters);
            inlen = rwio->read_len;
            inbytes = inlen;
            for (i=0;i<ilen;i++)
            {
                filterlist = bp_slist__index(rwio->read_filters,i);
                filter_node = (FILTER_FUNC *)filterlist->data;
                if (filter_node)
                {
                    if (filter_node->filter_func)
                    {
                        filterbytes = filter_node->filter_func(filter_node->filterhandle,
                                                               inbuf,
                                                               &outbuf,
                                                               inlen,
							       &inbytesused);
                        if (filterbytes <= 0)
                        {
                             if (!ifirst)
                                 lib_free(inbuf);
                             if (save_read_len)
                             {
                                 lib_free(save_read_buff);
                                 save_read_len = 0;
                             }
                             /* need more data */
                             goto readmore;
                         }
                         inlen = filterbytes;           
                     }
                 }
                 if (ifirst)
                 {
                     ifirst = 0;
                     if (inbytes == inbytesused)
                     {
                         save_read_buff = NULL;
                         save_read_len = 0;
                     }
                     else
                     {
                         save_read_len = inbytes-inbytesused;
                         save_read_buff = (char *)lib_malloc(save_read_len);
                     }
                     inbuf = outbuf;
                     outbuf = NULL;
                 }
                 else
                 {
                     lib_free(inbuf);
                     inbuf = outbuf;
                     outbuf = NULL;
                 }   
             }
         }
         /* ok move to filter buffer and reset read_block */
     ilen = rwio->filter_buff_len+filterbytes;
     outbuf = (char *)lib_malloc(ilen);
     memset(outbuf,'\0',ilen);
     if (rwio->filter_buff_len)
         memcpy(outbuf,rwio->filter_buff,rwio->filter_buff_len);
     memcpy(outbuf+rwio->filter_buff_len,inbuf,filterbytes);
     if (!ifirst)
         lib_free(inbuf);
     lib_free(rwio->filter_buff);
     rwio->filter_buff = outbuf;
     outbuf = NULL;
     rwio->filter_buff_len = ilen;
     if (save_read_len)
         memcpy(rwio->read_block,save_read_buff,save_read_len);
     rwio->read_len = save_read_len;
     }  while (1);
}


int blw_primary_reader(void *fd,char *buff,int bytes)
{
    int myfd;
    int ret;

    myfd = *(int *)fd;
    ret = recv(myfd, buff, bytes, 0);
#ifdef WIN32
    if (ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        ASSERT(err == WSAEWOULDBLOCK || err == WSAECONNABORTED);
    }
#endif

    return ret;
}


int blw_primary_writer(void *fd,char *buff,int bytes)
{
    int myfd;
    int ret;

    myfd = *(int *)fd;
    ret = send(myfd, buff, bytes, 0);
    return ret;
}

static void 
IW_fpollmgr_log (WRAPPER *wrap, char *action, int cc, char *buf) {
    char  c,
        *bp,
        *cp,
        *dp,
        *ep,  
        *fp,  
        buffer[BUFSIZ];

    sprintf (buffer, "%s %d octets", action, cc);
    wrap -> log(LOG_WRAP, 1, buffer);

    dp = buffer + sizeof buffer - 2;
    sprintf (bp = buffer, *action == 'r' ? "<<< " : ">>> ");
    fp = (bp += strlen (bp));

    for (ep = (cp = buf) + cc; cp < ep; cp++) {
        if ((c = *cp) == '\r') {
            *bp++ = '\\';
            c = 'r';
        }
        if (c != '\n')
            *bp++ = c;
        if ((c == '\n') || (bp >= dp)) {
            *bp = '\0';
            wrap -> log(LOG_WRAP, 0, buffer);
            sprintf (bp = buffer, *action == 'r' ? "<<< " : ">>> ");
            bp += strlen (bp);
        }
    }
    if (bp != fp) {
        *bp = '\0';
        wrap -> log(LOG_WRAP, 0, buffer);
    }
}


int IW_fpollmgr_move_to_beep_core(char *buffer,struct iovec *vector,int count)
{
    int bytes;

    bytes = vector[0].iov_len;
    if (bytes)
    {
        if (count <= bytes)
            memcpy(vector[0].iov_base,buffer,count);
        else
            memcpy(vector[0].iov_base,buffer,bytes);
    }
    return count-bytes;
}

char *IW_fpollmgr_move_from_beep_core(struct iovec *vector,int vec_count,int *size)
{
    int bytes,i,off=0;
    char *retbuf=NULL;


    bytes = 0;
    for (i=0;i<vec_count;i++)
        if (vector[i].iov_len > 0)
            bytes += vector[i].iov_len;
    if (bytes)
    {
        retbuf = (char *)lib_malloc(bytes);
        if (retbuf)
        {
            for (i=0;i<vec_count;i++)
            {
                if (vector[i].iov_len > 0)
                {
                    memcpy(retbuf+off,vector[i].iov_base,vector[i].iov_len);
                    off += vector[i].iov_len;
                }
            }
        }
    }
    *size = off;
    return retbuf;
}

void handle_write_event(IO_STATE* ios)
{
    WRAPPER* wrap = ios->wrapper;
    READER_WRITER *wrapio = (READER_WRITER *)ios->stack_reader_writer->data;
    struct beep_iovec *biv;
    int iKnt;

    SEM_WAIT(&ios->lock);
    if (ios->state == IOS_STATE_WRITE_PENDING) {
        ios->state = IOS_STATE_ACTIVE;
    }
    SEM_POST(&ios->lock);

    iKnt = 0;

    SEM_WAIT(&wrap->core_lock); 
    biv = bll_out_buffer(wrap->session);
    SEM_POST(&wrap->core_lock); 
    if (biv) {
        while (biv->vec_count) {
            int iLen;
            char *buff;

            iKnt = -1;

            SEM_WAIT(&wrap->core_lock);
            /* pull up iovec to a single buf for stacked write calls */
            buff = IW_fpollmgr_move_from_beep_core(biv->iovec,biv->vec_count,&iLen);
            if (buff)
            {
                iKnt = blw_writer(wrapio,buff,iLen);
                IW_fpollmgr_log (wrap, "wrote", iKnt, buff);
                lib_free(buff);
            }
            SEM_POST(&wrap->core_lock); 
            if (iKnt > 0)
            {
                SEM_WAIT(&wrap->core_lock); 
                bll_out_count(wrap->session, iKnt);
                biv = bll_out_buffer(wrap->session);
                SEM_POST(&wrap->core_lock);
                if (!biv)
                    break; 
            }
            else
                break;
        }
    }
    SEM_WAIT(&ios->lock);
    if (ios->state != IOS_STATE_WRITE_PENDING)
    {
        SEM_POST(&ios->lock);
        fiostate_block_write(ios);
        if (ios->state == IOS_STATE_CONNECTED)
            ios->state = IOS_STATE_GREETING_SENT;
        else
            if (ios->state == IOS_STATE_TEST_SEQ  &&
                wrap->conn.status == INST_STATUS_RUNNING)
                ios->state = IOS_STATE_ACTIVE;    
    } else {
        SEM_POST(&ios->lock);
    }
}

int handle_read_event(IO_STATE* ios)
{
    WRAPPER* wrap = ios->wrapper;
    READER_WRITER *wrapio = (READER_WRITER *)ios->stack_reader_writer->data;
    struct beep_iovec *biv;
    int eos = FALSE;
    int iKnt;
    char *buff;
    int iLen;


    SEM_WAIT(&wrap->core_lock);   
    biv = bll_in_buffer(wrap->session);
    SEM_POST(&wrap->core_lock); 
    while (biv)
    {
        /* connectstat->state == 2 is just for testing */
        if (ios->state == IOS_STATE_TEST_SEQ)
        {
            biv=NULL;
            fiostate_unblock_read(ios);
            continue;
        }

        /* this condition is for when the greeting has
         * been received but not processed and more data
         * is available. Hold the data in the socket until
         * the greeting has been processed
         */
        if (ios->state != IOS_STATE_GREETING_SENT &&
            wrap->conn.status == INST_STATUS_STARTING)
        {
            biv = NULL;
            ios->readable++;
            if (ios->readable > 200)
            {
                SEM_WAIT(&wrap->lock);
                wrap->conn.status = INST_STATUS_RUNNING;
                SEM_POST(&wrap->lock);
                ios->readable = 0;
            }
            continue;
        }
        iLen = 1;
        iKnt = (-1);
        SEM_WAIT(&wrap->core_lock);
        buff = NULL;
        /* check that the core is not out of memory */
        if (bll_status(wrap->session) <= 0)
        { 
            iLen = biv->iovec[0].iov_len;
            buff = lib_malloc(iLen);
            memset(buff,'\0',iLen);
            iKnt = blw_reader(wrapio,buff,iLen);
        }
        else 
        {
            biv = NULL;
        }
        SEM_POST(&wrap->core_lock); 
        if (iKnt > 0)
        {
            IW_fpollmgr_log (wrap, "read", iKnt, buff);
            SEM_WAIT(&wrap->core_lock);
            IW_fpollmgr_move_to_beep_core(buff,biv->iovec,iKnt);
            bll_in_count(wrap->session,iKnt);
            biv = bll_in_buffer(wrap->session);
            SEM_POST(&wrap->core_lock); 
            lib_free(buff);
        }
        else 
        {
            lib_free(buff);
            if (iKnt == 0) /* || iKnt == -1) */
                eos = TRUE;
            break;
        }
    }

    if (eos == FALSE)
    {
        SEM_WAIT(&ios->lock);
        if (ios->state == IOS_STATE_GREETING_SENT)
            /* set to 2 to test seq's */
            ios->state = IOS_STATE_ACTIVE;
        else if (wrap->conn.status == INST_STATUS_RUNNING)
            ios->state = IOS_STATE_ACTIVE;
        SEM_POST(&ios->lock);
    }

    return eos ? -1 : 0;
}

void handle_socket_error(IO_STATE* ios)
{
    WRAPPER* wrap = ios->wrapper;

    log_line(LOG_WRAP, 0, "socket error: %d", ios->socket);

    fiostate_stop(ios);
    ios->shutdown=1;
    wrap->conn.status = INST_STATUS_EXIT;
}

