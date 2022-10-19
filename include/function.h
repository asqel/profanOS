#ifndef FUNCTION_H
#define FUNCTION_H

/* Sometimes we want to keep parameters to a function for later use
 * and this is a solution to avoid the 'unused parameter' compiler warning */
#define UNUSED(x) (void)(x)
#define ARYLEN(x) (int)(sizeof(x) / sizeof((x)[0]))

#define NULL ((void *)0)

int pow(int base, int exp);
int abs(int x);

void init_rand();
int rand();

#endif
