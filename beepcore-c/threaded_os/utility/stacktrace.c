#ifdef LINUX
#define HAVE_BACKTRACE 1
#define HAVE_DLADDR 1
#define HAVE_DLFCN_H 1
#endif

void
bp_symbol_info (void *address,
                char **file_name,
                void **base_address,
                char **symbol_name,
                void **symbol_address,
                void **offset);
int bp_crawl_stack(void ***ptrarray, int length);

#if HAVE_DLFCN_H
#define _GNU_SOURCE
#include <dlfcn.h>
#endif
#if HAVE_BACKTRACE 
#include <execinfo.h>
#endif
#include <stdio.h>

#if HAVE_DLFCN_H
#if HAVE_BACKTRACE
int
bp_crawl_stack(void ***ptrarray, int length)
{
    return backtrace(*ptrarray, length);
}

#else

int
bp_crawl_stack( void ***ptrarray, int length)
{
    return 0;
}
#endif

#if HAVE_DLADDR
void
bp_symbol_info (void *address,
                char **file_name,
                void **base_address,
                char **symbol_name,
                void **symbol_address,
                void **offset)
{
    Dl_info sym_info;
    int ret;

    ret = dladdr(address, &sym_info);
    if (ret)
    {     
        *file_name = (char*)sym_info.dli_fname;
        *base_address = sym_info.dli_fbase;                               
        *symbol_name = (char*)sym_info.dli_sname;
        *symbol_address = sym_info.dli_saddr;                               
        *offset = (void*)((long)address - (long)*symbol_address);
    }
    else
    {
        *file_name = "????";
        *base_address = NULL;
        *symbol_name = "????";
        *symbol_address = NULL;
        *offset = NULL;
    }
}
#else /* HAVE_DLADDR */
void
bp_symbol_info (void *address,
                char **file_name,
                void **base_address,
                char **symbol_name,
                void **symbol_address,
                void **offset)
{
    *file_name = "????";
    *base_address = NULL;
    *symbol_name = "????";
    *symbol_address = NULL;
    *offset = NULL;
}
#endif /* HAVE_DLADDR */
#endif /* HAVE_DLFCN_H */

extern void bp_print_stack(char* prefix)
{
    void **trace, *address, *base_address, *symbol_address, *offset;
    char *file_name, *symbol_name;
    int count, i;


    trace = alloca(sizeof(void*) * 100);
    count = bp_crawl_stack((void***)&trace, 100);

    for (i = 0; i < count; i++)
    {
        address = trace[i];
        bp_symbol_info(address, &file_name, &base_address, &symbol_name,
                       &symbol_address, &offset);

        fprintf(stderr, "%s %p %s+%p (%s)\n", prefix, base_address, 
                symbol_name, offset, file_name);
    }
}

