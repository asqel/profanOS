#ifndef PANDA_ID
#define PANDA_ID 1005

#define get_func_addr ((int (*)(int, int)) *(int *) 0x1ffffb)

#define panda_print_string ((void (*)(char *, int, char)) get_func_addr(PANDA_ID, 9))
#define panda_get_cursor ((void (*)(uint32_t *, uint32_t *)) get_func_addr(PANDA_ID, 10))

#endif
