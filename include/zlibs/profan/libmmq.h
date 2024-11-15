/*****************************************************************************\
|   === libmmq.h : 2024 ===                                                   |
|                                                                             |
|    Kernel module header for minimalistic libC implementation     .pi0iq.    |
|                                                                 d"  . `'b   |
|    This file is part of profanOS and is released under          q. /|\  "   |
|    the terms of the GNU General Public License                   `// \\     |
|                                                                  //   \\    |
|   === elydre : https://github.com/elydre/profanOS ===         #######  \\   |
\*****************************************************************************/

#ifndef LIBMMQ_ID
#define LIBMMQ_ID 1001

#include <profan/type.h>

#define malloc(size) malloc_func(size, 0)
#define malloc_ask(size) malloc_func(size, 1)

#define calloc(nmemb, lsize) calloc_func(nmemb, lsize, 0)
#define calloc_ask(nmemb, lsize) calloc_func(nmemb, lsize, 1)

#define realloc(mem, new_size) realloc_func(mem, new_size, 0)
#define realloc_ask(mem, new_size) realloc_func(mem, new_size, 1)

#define exit(code) syscall_process_exit(syscall_process_pid(), code, 0)

#define get_func_addr ((uint32_t (*)(uint32_t, uint32_t)) *(uint32_t *) 0x1ffffb)

#define calloc_func ((void *(*)(uint32_t, uint32_t, int)) get_func_addr(LIBMMQ_ID, 2))
#define free ((void (*)(void *)) get_func_addr(LIBMMQ_ID, 3))
#define realloc_func ((void *(*)(void *, uint32_t, int)) get_func_addr(LIBMMQ_ID, 4))
#define malloc_func ((void *(*)(uint32_t, int)) get_func_addr(LIBMMQ_ID, 5))
#define memcpy ((void *(*)(void *, const void *, size_t)) get_func_addr(LIBMMQ_ID, 6))
#define memcmp ((int (*)(const void *, const void *, size_t)) get_func_addr(LIBMMQ_ID, 7))
#define memset ((void *(*)(void *, int, size_t)) get_func_addr(LIBMMQ_ID, 8))
#define memmove ((void *(*)(void *, const void *, size_t)) get_func_addr(LIBMMQ_ID, 9))
#define strcmp ((int (*)(const char *, const char *)) get_func_addr(LIBMMQ_ID, 10))
#define strcpy ((int (*)(char *, const char *)) get_func_addr(LIBMMQ_ID, 11))
#define strlen ((size_t (*)(const char *)) get_func_addr(LIBMMQ_ID, 12))
#define strdup ((char *(*)(const char *)) get_func_addr(LIBMMQ_ID, 13))
#define strncpy ((char *(*)(char *, const char *, size_t)) get_func_addr(LIBMMQ_ID, 14))
#define strcat ((char *(*)(char *, const char *)) get_func_addr(LIBMMQ_ID, 15))
#define strncmp ((int (*)(const char *, const char *, size_t)) get_func_addr(LIBMMQ_ID, 16))
#define fd_putchar ((void (*)(int, char)) get_func_addr(LIBMMQ_ID, 17))
#define fd_putstr ((void (*)(int, const char *)) get_func_addr(LIBMMQ_ID, 18))
#define fd_putint ((void (*)(int, int)) get_func_addr(LIBMMQ_ID, 19))
#define fd_puthex ((void (*)(int, uint32_t)) get_func_addr(LIBMMQ_ID, 20))
#define fd_printf ((void (*)(int, const char *, ...)) get_func_addr(LIBMMQ_ID, 21))
#define atoi ((int (*)(const char *)) get_func_addr(LIBMMQ_ID, 22))

#endif
