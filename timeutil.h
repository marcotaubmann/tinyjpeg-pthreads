#ifndef __TIMEUTIL_H__
#define __TIMEUTIL_H__

#include <sys/time.h>

typedef struct timeval timer;
#define TIME(x) gettimeofday(&x, NULL);


long timevaldiff(timer *start, timer *finish);
void printtime(char *action, timer *start, timer *finish);



#endif
